"""CUDA Driver migration using frozen compiler output to isolate API effects.

OpenCL is needed only to regenerate this build artifact. The CUDA execution path
does not load OpenCL.dll. Preserve arithmetic; replace only the zero global offset
and add graph variants that read the same host-computed angle from device memory.
"""
import hashlib
import json
import pathlib
import re

root=pathlib.Path(__file__).resolve().parents[1]
source=root/'Intermediate/CUDA/generated/opencl_binary.bin'
ptx=source.read_bytes().rstrip(b'\0').decode()
assert set(re.findall(r'%envreg\d+',ptx))=={'%envreg3'}
ptx=ptx.replace('%envreg3','0')
for name, index in [('solve',11),('velocity',6)]:
    start=ptx.index('.entry '+name+'(')
    opening=ptx.index('{',start)
    depth=1;end=opening+1
    while depth:
        depth+=(ptx[end]=='{')-(ptx[end]=='}');end+=1
    text=ptx[start:end]
    parameter=name+'_param_'+str(index)
    text=text.replace('.param .f32 '+parameter+',',
                      '.param .u64 '+parameter+',\n\t.param .u32 cuda_angle_index,')
    text=text.replace('{','{\n.reg .b64 %cuda_angle_ptr, %cuda_angle_offset, %cuda_angle_addr;\n.reg .b32 %cuda_angle_i;',1)
    pattern=r'ld.param.f32\s+(%f\d+), \['+parameter+r'\];'
    matches=re.findall(pattern,text)
    assert len(matches)==1, (name,matches)
    instructions=(f'ld.param.u64 %cuda_angle_ptr, [{parameter}];\n'
                  'ld.param.u32 %cuda_angle_i, [cuda_angle_index];\n'
                  'mul.wide.u32 %cuda_angle_offset, %cuda_angle_i, 4;\n'
                  'add.u64 %cuda_angle_addr, %cuda_angle_ptr, %cuda_angle_offset;\n'
                  f'ld.global.f32 {matches[0]}, [%cuda_angle_addr];')
    text=re.sub(pattern,instructions,text)
    text=text.replace('.entry '+name+'(','.entry '+name+'_graph(')
    ptx+='\n'+text+'\n'
target=root/'SourceAssets/PowderCUDA/plant_sm89.ptx'
target.parent.mkdir(exist_ok=True)
target.write_text(ptx)
record=dict(compiler='NVIDIA OpenCL compiler CL-38566155; frozen PTX 9.3, sm_89',
            execution='CUDA Driver API, no OpenCL runtime dependency',
            source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
            artifact_sha256=hashlib.sha256(target.read_bytes()).hexdigest(),
            transformations=['OpenCL global offset is zero','graph angle supplied from a device array; unchanged host accumulation'],
            source='Intermediate/CUDA/generated/plant_preprocess.cpp',
            regeneration=['python -B Scripts/cuda_generate_kernels.py','python -B Scripts/cuda_opencl_inspect.py','python -B Scripts/cuda_prepare_ptx.py'])
(root/'Docs/CUDA20261118/ptx_provenance.json').write_text(json.dumps(record,indent=2))
print(target,len(ptx))
