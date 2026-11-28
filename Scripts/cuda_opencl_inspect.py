"""Read-only compiler diagnostic for the existing NVIDIA OpenCL implementation."""
import ctypes as c
import pathlib
import sys
root=pathlib.Path(__file__).resolve().parents[1]
cl=c.WinDLL('OpenCL.dll')
def bind(name,result,args):
    f=getattr(cl,name);f.restype=result;f.argtypes=args;return f
ptr=c.c_void_p;u=c.c_uint;i=c.c_int;size=c.c_size_t
platforms=bind('clGetPlatformIDs',i,[u,ptr,ptr])
devices=bind('clGetDeviceIDs',i,[ptr,c.c_ulonglong,u,ptr,ptr])
context=bind('clCreateContext',ptr,[ptr,u,ptr,ptr,ptr,ptr])
program=bind('clCreateProgramWithSource',ptr,[ptr,u,ptr,ptr,ptr])
build=bind('clBuildProgram',i,[ptr,u,ptr,c.c_char_p,ptr,ptr])
info=bind('clGetProgramInfo',i,[ptr,u,size,ptr,ptr])
release_program=bind('clReleaseProgram',i,[ptr])
release_context=bind('clReleaseContext',i,[ptr])
def check(n):
    if n:raise RuntimeError(n)
np=u();check(platforms(0,None,c.byref(np)));ps=(ptr*np.value)();check(platforms(np,ps,None))
dev=ptr()
for p in ps:
    if devices(p,4,1,c.byref(dev),None)==0:break
error=i();ctx=context(None,1,c.byref(dev),None,None,c.byref(error));check(error.value)
math_only=len(sys.argv)>1 and sys.argv[1]=='math'
source=(b'''__kernel void math3(__global const float4* p,__global float4* out){int i=get_global_id(0);float3 a=p[2*i].xyz,b=p[2*i+1].xyz;out[2*i]=(float4)(dot(a,b),length(a),length(a.xy),dot(a.xy,b.xy));out[2*i+1]=(float4)(normalize(a),a.x/b.x);}
__kernel void len3(__global const float4* p,__global float* out){out[get_global_id(0)]=length(p[get_global_id(0)].xyz);}
__kernel void len2(__global const float4* p,__global float* out){out[get_global_id(0)]=length(p[get_global_id(0)].xy);}
__kernel void dot3(__global const float4* p,__global float* out){out[get_global_id(0)]=dot(p[0].xyz,p[1].xyz);}
__kernel void norm3(__global const float4* p,__global float4* out){out[get_global_id(0)]=(float4)(normalize(p[get_global_id(0)].xyz),0);}
''' if math_only else (root/'Intermediate/CUDA/generated/plant_preprocess.cpp').read_bytes());src=c.c_char_p(source)
prog=program(ctx,1,c.byref(src),None,c.byref(error));check(error.value)
try:
    check(build(prog,1,c.byref(dev),b'-cl-std=CL1.2 -DGRANULAR_PLANT=1 -DGRANULAR_BOUNDS_FAST=1 -DGRANULAR_KERNEL_OPTIMIZE=1 -DPLANT_WALL_MODE=5 -DPLANT_THREADED=1',None,None))
    length=size();check(info(prog,0x1165,c.sizeof(length),c.byref(length),None))
    binary=c.create_string_buffer(length.value);address=c.cast(binary,ptr)
    check(info(prog,0x1166,c.sizeof(ptr),c.byref(address),None))
    dest=root/'Intermediate/CUDA/generated'/('opencl_math.ptx' if math_only else 'opencl_binary.bin');dest.write_bytes(binary.raw)
    print('OpenCL compiler binary',dest,length.value,'prefix',binary.raw[:80])
finally:
    release_program(prog);release_context(ctx)
