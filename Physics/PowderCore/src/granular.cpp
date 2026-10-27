#define CL_TARGET_OPENCL_VERSION 120
#include "powder/granular.hpp"
#include <CL/cl.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <set>
#include <map>
#include <algorithm>
#include <stdexcept>
namespace powder {
namespace {
using Clock=std::chrono::steady_clock;using V=std::array<double,4>;
double elapsed(Clock::time_point t){return std::chrono::duration<double>(Clock::now()-t).count();}
void require(bool b,const char* s){if(!b)throw std::invalid_argument(s);}
void check(cl_int code,const char* op){if(code)throw std::runtime_error(std::string(op)+" OpenCL error "+std::to_string(code));}
#include "granular_mx301_cpu.inc"
const char* source=
#include "granular_mx301_cl.inc"

#include "granular_kernels.inc"
;
#define GPU_API(X) X(clGetPlatformIDs) X(clGetDeviceIDs) X(clGetDeviceInfo) X(clCreateContext) X(clCreateCommandQueue) X(clCreateProgramWithSource) X(clBuildProgram) X(clGetProgramBuildInfo) X(clCreateKernel) X(clCreateBuffer) X(clSetKernelArg) X(clEnqueueWriteBuffer) X(clEnqueueReadBuffer) X(clEnqueueNDRangeKernel) X(clFinish) X(clGetEventProfilingInfo) X(clReleaseEvent) X(clReleaseMemObject) X(clReleaseKernel) X(clReleaseProgram) X(clReleaseCommandQueue) X(clReleaseContext)
struct Api {
 HMODULE module=nullptr;
 #define DECLARE(name) decltype(&::name) name=nullptr;
 GPU_API(DECLARE)
 #undef DECLARE
 Api(){module=LoadLibraryExW(L"OpenCL.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!module)throw std::runtime_error("system OpenCL runtime unavailable");
 #define LOAD(name) name=reinterpret_cast<decltype(name)>(GetProcAddress(module,#name));if(!name){FreeLibrary(module);module=nullptr;throw std::runtime_error("missing OpenCL entry " #name);}
 GPU_API(LOAD)
 #undef LOAD
 }
 ~Api(){if(module)FreeLibrary(module);}
};
}
struct GranularGPU::Impl {
 Api api;cl_device_id device=nullptr;cl_context context=nullptr;cl_command_queue queue=nullptr;cl_program program=nullptr;
 std::array<cl_kernel,6> kernels{};std::array<cl_mem,8> buffers{};
 GranularConfig c;int nx=0,ny=0,nz=0,cells=0;float cell=0,angle=0;double time=0;std::string name;bool poisoned=false;GranularTiming timing;std::vector<std::pair<cl_event,unsigned>> events;
 Impl(GranularConfig cfg):c(cfg){try{
 require(c.count>=1&&c.count<=1000000&&c.iterations>=2&&c.iterations<=32,"granular count/iterations outside contract");
 if(c.radius==0)c.radius=static_cast<float>(std::cbrt(.054/(c.count*4.188790204786391)));
 require(std::isfinite(c.radius)&&c.radius>.001f&&c.radius<.03f&&std::isfinite(c.dt)&&c.dt>0&&c.dt<=.001f&&std::isfinite(c.friction)&&c.friction>=0&&c.friction<=1,"invalid granular configuration");
 cl_uint count=0;check(api.clGetPlatformIDs(0,nullptr,&count),"platform count");std::vector<cl_platform_id> platforms(count);check(api.clGetPlatformIDs(count,platforms.data(),nullptr),"platforms");
 for(auto platform:platforms){cl_uint n=0;if(api.clGetDeviceIDs(platform,CL_DEVICE_TYPE_GPU,0,nullptr,&n)||!n)continue;std::vector<cl_device_id> ids(n);check(api.clGetDeviceIDs(platform,CL_DEVICE_TYPE_GPU,n,ids.data(),nullptr),"devices");device=ids[0];break;}require(device!=nullptr,"no OpenCL GPU");
 char text[256]{};check(api.clGetDeviceInfo(device,CL_DEVICE_NAME,sizeof(text),text,nullptr),"device name");name=text;
 cl_int err=0;context=api.clCreateContext(nullptr,1,&device,nullptr,nullptr,&err);check(err,"context");queue=api.clCreateCommandQueue(context,device,c.profile?CL_QUEUE_PROFILING_ENABLE:0,&err);check(err,"queue");
 program=api.clCreateProgramWithSource(context,1,&source,nullptr,&err);check(err,"program");std::string options=std::string("-cl-std=CL1.2 -DGRANULAR_BOUNDS_FAST=")+(c.fast_bounds?"1":"0")+" -DGRANULAR_MX301="+(c.test_mixer?"1":"0");err=api.clBuildProgram(program,1,&device,options.c_str(),nullptr,nullptr);if(err){std::size_t size=0;api.clGetProgramBuildInfo(program,device,CL_PROGRAM_BUILD_LOG,0,nullptr,&size);std::string log(size,' ');api.clGetProgramBuildInfo(program,device,CL_PROGRAM_BUILD_LOG,size,log.data(),nullptr);throw std::runtime_error(log);}
 const char* names[]={"cleargrid","grid","predict","solve","velocity"};for(unsigned k=0;k<5;k++){kernels[k]=api.clCreateKernel(program,names[k],&err);check(err,"kernel");}
 cell=2*c.radius;nx=static_cast<int>(std::ceil((c.test_mixer?1.6:(c.fast_bounds?.92:1.2))/cell));ny=static_cast<int>(std::ceil((c.test_mixer?1.0:(c.fast_bounds?.52:.8))/cell));nz=static_cast<int>(std::ceil((c.test_mixer?1.0:(c.fast_bounds?1.02:1.2))/cell));cells=nx*ny*nz;
 for(unsigned i=0;i<8;i++){std::size_t bytes=(i<4||i==7)?c.count*sizeof(Grain):i==4?cells*sizeof(int):i==5?c.count*sizeof(int):sizeof(int);buffers[i]=api.clCreateBuffer(context,CL_MEM_READ_WRITE,bytes,nullptr,&err);check(err,"buffer");}
 std::vector<Grain> initial;initial.reserve(c.count);float pitch=2.04f*c.radius;
 if(!c.test_mixer)for(float z=c.radius;z<.4f-c.radius&&initial.size()<c.count;z+=pitch)for(float y=-.25f+c.radius;y<.25f-c.radius&&initial.size()<c.count;y+=pitch)for(float x=-.45f+c.radius;x<.45f-c.radius&&initial.size()<c.count;x+=pitch){
  bool hit=std::hypot(y,z-.21f)<.025f+c.radius;
  for(unsigned k=0;k<6;k++){float a=k*1.04719755f,co=std::cos(a),si=std::sin(a);float lx=x-(-.375f+k*.15f),ly=co*y+si*(z-.21f)-.095f,lz=-si*y+co*(z-.21f);float beta=k%2?-.4f:.4f;float tx=std::cos(beta)*lx+std::sin(beta)*ly,ty=-std::sin(beta)*lx+std::cos(beta)*ly;float dx=std::max(0.f,std::abs(tx)-.045f),dy=std::max(0.f,std::abs(ty)-.07f),dz=std::max(0.f,std::abs(lz)-.012f);hit=hit||dx*dx+dy*dy+dz*dz<c.radius*c.radius;}
  if(!hit)initial.push_back({x,y,z,x<0?0.f:1.f});
 }
 if(c.test_mixer){for(float z=-.325f+c.radius;z<.325f-c.radius&&initial.size()<c.count;z+=pitch)for(float y=-.45f+c.radius;y<.45f-c.radius&&initial.size()<c.count;y+=pitch)for(float x=-.75f+c.radius;x<.75f-c.radius&&initial.size()<c.count;x+=pitch){
  bool hit=false;for(int j=0;j<133;j++){auto a=mxplanes[j];if(a[0]*x+a[1]*y+a[2]*z-a[3]>-c.radius){hit=true;break;}}
  if(!hit)for(auto& part:mxparts){float distance=-1.e9f;for(int j=int(part[0]);j<int(part[0]+part[1]);j++){auto a=mxplanes[j];distance=std::max(distance,a[0]*x+a[1]*y+a[2]*z-a[3]);}if(distance<c.radius){hit=true;break;}}
  if(!hit)initial.push_back({x,y,z,x<0?0.f:1.f});
 }}
 require(initial.size()==c.count,"initial charge does not fit vessel");
 // MX301 charge is generated bottom-to-top: red lower half, blue upper half.
 // Keep positions and equal material counts; only the interface layer may share both groups.
 if(c.test_mixer)for(std::size_t i=0;i<initial.size();++i)initial[i].w=i<initial.size()/2?0.f:1.f;
 std::vector<Grain> v(c.count);set(initial,v);
 }catch(...){release();throw;}}
 void release(){for(auto e:events)if(e.first)api.clReleaseEvent(e.first);events.clear();for(auto k:kernels)if(k)api.clReleaseKernel(k);for(auto b:buffers)if(b)api.clReleaseMemObject(b);if(program)api.clReleaseProgram(program);if(queue)api.clReleaseCommandQueue(queue);if(context)api.clReleaseContext(context);}
 ~Impl(){release();}
 template<class T>void arg(unsigned k,unsigned i,const T& v){check(api.clSetKernelArg(kernels[k],i,sizeof(v),&v),"arg");}
 void run(unsigned k,std::size_t n){std::size_t local=128,global=(n+127)/128*128;cl_event event=nullptr;check(api.clEnqueueNDRangeKernel(queue,kernels[k],1,nullptr,&global,&local,0,nullptr,c.profile?&event:nullptr),"dispatch");if(event)events.push_back({event,k});}
 void set(const std::vector<Grain>& state,const std::vector<Grain>& vel){require(state.size()==c.count&&vel.size()==c.count,"state count mismatch");for(unsigned i=0;i<c.count;i++)for(float v:{state[i].x,state[i].y,state[i].z,state[i].w,vel[i].x,vel[i].y,vel[i].z})require(std::isfinite(v),"nonfinite input state");
 check(api.clEnqueueWriteBuffer(queue,buffers[0],CL_TRUE,0,state.size()*sizeof(Grain),state.data(),0,nullptr,nullptr),"state write");check(api.clEnqueueWriteBuffer(queue,buffers[2],CL_TRUE,0,vel.size()*sizeof(Grain),vel.data(),0,nullptr,nullptr),"velocity write");time=0;angle=0;poisoned=false;for(auto e:events)if(e.first)api.clReleaseEvent(e.first);events.clear();}
 void advance(unsigned steps,float rpm,bool gate){auto begin=Clock::now();require(!poisoned,"reset required after rejected GPU batch");require(steps>0&&steps<=1000&&std::isfinite(rpm)&&std::abs(rpm)<=60,"invalid granular control");int zero=0;check(api.clEnqueueWriteBuffer(queue,buffers[6],CL_FALSE,0,sizeof(int),&zero,0,nullptr,nullptr),"clear error");int n=static_cast<int>(c.count),g=gate?1:0;float omega=rpm*.104719755f;
 for(unsigned s=0;s<steps;s++){
  angle+=omega*c.dt;
  arg(2,0,buffers[0]);arg(2,1,buffers[1]);arg(2,2,buffers[2]);arg(2,3,n);arg(2,4,c.dt);arg(2,5,c.radius);arg(2,6,buffers[6]);arg(2,7,buffers[7]);run(2,c.count);
  for(unsigned it=0;it<c.iterations;it++){
   arg(0,0,buffers[4]);arg(0,1,cells);run(0,cells);
   arg(1,0,buffers[0]);arg(1,1,buffers[4]);arg(1,2,buffers[5]);arg(1,3,n);arg(1,4,cell);arg(1,5,nx);arg(1,6,ny);arg(1,7,nz);arg(1,8,buffers[6]);run(1,c.count);
   arg(3,0,buffers[0]);arg(3,1,buffers[1]);arg(3,2,buffers[3]);arg(3,3,buffers[4]);arg(3,4,buffers[5]);arg(3,5,n);arg(3,6,c.radius);arg(3,7,cell);arg(3,8,nx);arg(3,9,ny);arg(3,10,nz);arg(3,11,angle);arg(3,12,g);arg(3,13,c.friction);run(3,c.count);std::swap(buffers[0],buffers[3]);
  }
  arg(4,0,buffers[0]);arg(4,1,buffers[1]);arg(4,2,buffers[2]);arg(4,3,n);arg(4,4,c.dt);arg(4,5,c.radius);arg(4,6,angle);arg(4,7,omega);arg(4,8,g);arg(4,9,c.friction);arg(4,10,buffers[6]);arg(4,11,buffers[7]);run(4,c.count);
 }
 int error=0;check(api.clEnqueueReadBuffer(queue,buffers[6],CL_TRUE,0,sizeof(int),&error,0,nullptr,nullptr),"guard");if(error)throw std::runtime_error("granular safety rejection bits="+std::to_string(error)+" (1=domain,2=step displacement,4=nonfinite); reset required");time+=steps*c.dt;timing.advance_wall+=elapsed(begin);for(auto& e:events){cl_ulong start=0,end=0;check(api.clGetEventProfilingInfo(e.first,CL_PROFILING_COMMAND_START,sizeof(start),&start,nullptr),"profile start");check(api.clGetEventProfilingInfo(e.first,CL_PROFILING_COMMAND_END,sizeof(end),&end,nullptr),"profile end");timing.gpu_seconds[e.second]+=(end-start)*1.e-9;api.clReleaseEvent(e.first);e.first=nullptr;}events.clear();
 }
};
GranularGPU::GranularGPU(GranularConfig c):p(std::make_unique<Impl>(c)){}
GranularGPU::~GranularGPU()=default;
void GranularGPU::advance(unsigned s,float rpm,bool gate){try{p->advance(s,rpm,gate);}catch(...){p->poisoned=true;throw;}}
std::vector<Grain> GranularGPU::snapshot(){auto begin=Clock::now();std::vector<Grain> result(p->c.count);check(p->api.clEnqueueReadBuffer(p->queue,p->buffers[0],CL_TRUE,0,result.size()*sizeof(Grain),result.data(),0,nullptr,nullptr),"snapshot");p->timing.snapshot_wall+=elapsed(begin);return result;}
void GranularGPU::set_state(const std::vector<Grain>& a,const std::vector<Grain>& b){p->set(a,b);}
GranularTiming GranularGPU::timing()const{return p->timing;}
double GranularGPU::time()const{return p->time;}
float GranularGPU::radius()const{return p->c.radius;}
float GranularGPU::angle()const{return p->angle;}
std::string GranularGPU::device()const{return p->name;}
}
