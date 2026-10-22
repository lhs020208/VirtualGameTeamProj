"""Windows GPU/reference validation. Full target is always 1,591,550 real spheres."""
import argparse,datetime,hashlib,json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CMAKE=Path('C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--full-target',action='store_true');args=ap.parse_args()
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False);build=ROOT/'Intermediate/PowderCore';exe=build/'Release'
    report={'schema':1,'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'validation_passed':False,'target_accepted':False,'unreal_tested':False,'runs':[]}
    def run(command,name,timeout=300):
        with (out/name).open('w',encoding='utf-8') as log:
            result=subprocess.run(list(map(str,command)),cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=timeout,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        report['runs'].append({'command':list(map(str,command)),'exit_code':result.returncode,'log':name})
        if result.returncode:raise RuntimeError(name+' failed')
    try:
        run([CMAKE,'-S',ROOT/'Physics/PowderCore','-B',build,'-DBUILD_TESTING=ON'],'configure.log')
        run([CMAKE,'--build',build,'--config','Release'],'build.log')
        run([CMAKE.with_name('ctest.exe'),'--test-dir',build,'-C','Release','--output-on-failure'],'ctest.log',600)
        for name in ['gpu_free_tests','gpu_contact_tests','gpu_wall_tests']:
            run([exe/(name+'.exe'),out/name],name+'.log')
        run([exe/'process_tests.exe'],'process_tests.log')
        if args.full_target:run([exe/'powder_gpu18.exe',out/'full_target'],'full_target.log',180)
        report['validation_passed']=True
    finally:
        files={}
        for folder in ['Physics/PowderCore','Source/VirtualGameTeamProj']:
            for f in (ROOT/folder).rglob('*'):
                if f.is_file() and f.suffix in {'.hpp','.h','.cpp','.inc','.cs','.txt'}:files[f.relative_to(ROOT).as_posix()]=hashlib.sha256(f.read_bytes()).hexdigest()
        report['source_sha256']=files
        (out/'validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
if __name__=='__main__':main()
