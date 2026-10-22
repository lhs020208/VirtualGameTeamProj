"""Build/test optimization19-24. Full target requires ~8.1GB device allocation; close editor before timing."""
import argparse,datetime,hashlib,json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CMAKE=Path('C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--full-target',action='store_true');a=p.parse_args()
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);build=ROOT/'Intermediate/PowderCore'
 report={'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'validation_passed':False,'target_accepted':False,'unreal_tested':False,'runs':[]}
 def run(cmd,name,timeout=600):
  with (out/name).open('w',encoding='utf-8') as log:r=subprocess.run(list(map(str,cmd)),cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=timeout,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
  report['runs'].append({'command':list(map(str,cmd)),'exit_code':r.returncode,'log':name})
  if r.returncode:raise RuntimeError(name+' failed')
 try:
  run([CMAKE,'-S',ROOT/'Physics/PowderCore','-B',build,'-DBUILD_TESTING=ON'],'configure.log')
  run([CMAKE,'--build',build,'--config','Release'],'build.log')
  run([CMAKE.with_name('ctest.exe'),'--test-dir',build,'-C','Release','--output-on-failure'],'ctest.log')
  run([sys.executable,'-X','utf8',ROOT/'Scripts/test_optimization24_audit.py'],'audit_test.log')
  if a.full_target:
   run([build/'Release/powder_gpu24.exe',out/'full_target'],'full_target.log',900)
   run([sys.executable,'-X','utf8',ROOT/'Scripts/audit_optimization24.py',ROOT/'Docs/DEM/optimization19_controlled/windows.csv',out/'full_target/windows.csv',out/'comparison.json'],'comparison.log')
  report['validation_passed']=True
 finally:
  report['source_sha256']={f.relative_to(ROOT).as_posix():hashlib.sha256(f.read_bytes()).hexdigest() for folder in ['Physics/PowderCore','Source/VirtualGameTeamProj','Scripts'] for f in (ROOT/folder).rglob('*') if f.is_file() and f.suffix in {'.hpp','.h','.cpp','.inc','.cs','.txt','.py'}}
  (out/'validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
if __name__=='__main__':main()
