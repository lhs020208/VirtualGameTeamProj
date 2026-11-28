import pathlib,subprocess,os,sys,json
root=pathlib.Path(__file__).resolve().parents[1]
vs=pathlib.Path('C:/Program Files/Microsoft Visual Studio/2022/Community')
vc=vs/'VC/Auxiliary/Build/vcvars64.bat'
envbat=root/'Intermediate/CUDA/environment.bat';envbat.parent.mkdir(parents=True,exist_ok=True)
envbat.write_text('@echo off\ncall "'+str(vc)+'" >nul\nset\n')
result=subprocess.check_output(['cmd.exe','/d','/c',str(envbat)],text=True)
env=os.environ.copy()
for line in result.splitlines():
    if '=' in line and not line.startswith('='):
        k,v=line.split('=',1);env[k]=v
env['VSLANG']='1033'
probe_dir=root/'Intermediate/CUDA'
probe_header=probe_dir/'cuda_dependency_probe.hpp';probe_header.write_text('// dependency prefix probe\n')
probe_source=probe_dir/'cuda_dependency_probe.cpp';probe_source.write_text('#include "cuda_dependency_probe.hpp"\n')
compiler=pathlib.Path(env['VCToolsInstallDir'])/'bin/Hostx64/x64/cl.exe'
probe=subprocess.run([str(compiler),'/nologo','/E','/showIncludes',str(probe_source)],env=env,capture_output=True)
probe_bytes=probe.stdout+probe.stderr
try:probe_text=probe_bytes.decode('utf-8')
except UnicodeDecodeError:probe_text=probe_bytes.decode('cp949')
for line in probe_text.splitlines():
    if 'cuda_dependency_probe.hpp' in line and not line.startswith('#'):
        prefix=line[:line.index(str(probe_header))] if str(probe_header) in line else None
        if prefix:env['POWDER_MSVC_INCLUDE_PREFIX']=prefix
if 'POWDER_MSVC_INCLUDE_PREFIX' not in env:raise RuntimeError('Cannot determine MSVC dependency prefix')
cmake=vs/'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
ninja=vs/'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
kit=root/'Intermediate/CUDAToolkit'
env['PATH']=str(kit/'bin')+';'+env.get('Path',env.get('PATH',''))
mode=sys.argv[1];isbase=mode.startswith('baseline');build=root/'Intermediate'/('CUDABaseline' if isbase else 'CUDA')
source=root/('Intermediate/CUDABaselineSource' if isbase else 'Physics/PowderCore')
if mode in ('configure','baseline'):
    args=[str(cmake),'-S',str(source),'-B',str(build),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_MAKE_PROGRAM='+str(ninja),'-DCMAKE_CXX_COMPILER=cl','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL']
    if mode=='configure': args+=['-DPOWDER_ENABLE_CUDA=ON','-DCUDAToolkit_ROOT='+str(kit),'-DCMAKE_CUDA_COMPILER='+str(kit/'bin/nvcc.exe')]
else: args=[str(cmake),'--build',str(build),'--parallel','2',*(['--clean-first'] if mode=='rebuild' else []),*(['--target',*sys.argv[2:]] if len(sys.argv)>2 else [])]
print('Building/configuring',mode,sys.argv[2:],flush=True)
log=root/'Docs/CUDA20261118'/('build_'+mode+'.log')
with log.open('w',encoding='utf-8') as f:
    proc=subprocess.Popen(args,env=env,cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    lines=[]
    for raw in proc.stdout:
        line=raw.decode('utf-8',errors='replace');f.write(line);lines.append(line)
    code=proc.wait()
for line in lines[-45:] if code else [x for x in lines if x.startswith('[') or x.startswith('--')][-15:]: print(line,end='')
print('exit',code,'log',log,flush=True)
sys.exit(code)
