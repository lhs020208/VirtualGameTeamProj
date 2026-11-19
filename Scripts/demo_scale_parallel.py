"""Temporary unchanged-equation contact-loop parallelism experiment, never installed."""
import pathlib,subprocess,os,sys,json
root=pathlib.Path(__file__).resolve().parents[1];tmp=root/'Intermediate/DemoScale200k';out=root/'Docs/DemoScale200k'
include=tmp/'parallel_include/powder';include.mkdir(parents=True,exist_ok=True)
header=(root/'Physics/PowderCore/include/powder/demo_plant.hpp').read_text()
needle='for(unsigned i=0;i<pos.size();++i){correction[i]={};'
assert header.count(needle)==1
header=header.replace('#include "plant.hpp"','#include "powder/plant.hpp"').replace(needle,'\n#pragma omp parallel for schedule(static) if(pos.size()>8192)\n    for(int i=0;i<int(pos.size());++i){correction[i]={};')
(include/'demo_plant.hpp').write_text(header)
code=(root/'Physics/PowderCore/tests/demo_scale_bench.cpp').read_text()
needle='std::cerr<<std::setprecision(6)<<"sim="'
assert needle in code
code=code.replace(needle,'std::uint64_t digest=1469598103934665603ull;for(unsigned char byte:std::vector<unsigned char>(reinterpret_cast<const unsigned char*>(state.data()),reinterpret_cast<const unsigned char*>(state.data())+state.size()*sizeof(powder::Grain)))digest=(digest^byte)*1099511628211ull;\n   std::cerr<<"hash="<<digest<<" "<<std::setprecision(9)<<"sim="')
cpp=tmp/'parallel_bench.cpp';cpp.write_text(code)
bat=tmp/'environment.bat';bat.write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\nset\n')
env=os.environ.copy()
for line in subprocess.check_output(['cmd.exe','/d','/c',str(bat)]).decode('utf-8',errors='replace').splitlines():
 if '=' in line:k,v=line.split('=',1);env[k]=v
cl=pathlib.Path(env['VCToolsInstallDir'])/'bin/Hostx64/x64/cl.exe'
for name,extra in [('reference_hash',[]),('parallel',['/openmp','/I'+str(include.parent)])]:
 exe=tmp/(name+'.exe')
 subprocess.run([str(cl),'/nologo','/std:c++17','/EHsc','/O2','/MD',*extra,'/I'+str(root/'Physics/PowderCore/include'),str(cpp),'/Fe:'+str(exe),'/Fo:'+str(tmp/(name+'.obj'))],env=env,cwd=tmp,check=True)
if len(sys.argv)>1 and sys.argv[1]=='build':raise SystemExit(0)
for name,seconds in [('reference_hash',50),('parallel',90)]:
 env['OMP_NUM_THREADS']='12';env['OMP_DYNAMIC']='FALSE'
 with (out/(name+'.json')).open('w') as result,(out/(name+'.progress.txt')).open('w') as progress:
  p=subprocess.Popen([str(tmp/(name+'.exe')),str(root),str(tmp/'charge_200k.csv'),str(seconds),'180'],stdout=result,stderr=progress,env=env,creationflags=0x08000000)
  print('OWN_NATIVE',name,p.pid,flush=True);code=p.wait()
 if code:raise SystemExit(code)
 print((out/(name+'.json')).read_text(),flush=True)
def state_at_50(name):
 for line in (out/(name+'.progress.txt')).read_text().splitlines():
  fields=dict(x.split('=',1) for x in line.split())
  if 50<=float(fields['sim'])<50.01:return fields['hash']
 raise ValueError('missing 50-second state')
a=state_at_50('reference_hash');b=state_at_50('parallel')
(out/'parallel_validation.json').write_text(json.dumps(dict(threads=12,same_equations=True,reference_hash=a,parallel_hash=b,bitwise_identical_at_50s=a==b,installed=False),indent=2))
assert a==b,'parallelism changed state'
