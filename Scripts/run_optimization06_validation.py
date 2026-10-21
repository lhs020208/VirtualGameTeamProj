"""Replay the standalone thread/physics contract; Unreal automation is a separate engine test."""
from pathlib import Path
import argparse,hashlib,json,re,subprocess
def main():
 root=Path(__file__).resolve().parents[1]
 p=argparse.ArgumentParser();p.add_argument('--exe',type=Path,default=root/'Intermediate/PowderCore/Release/clump_worker_tests.exe');p.add_argument('--out',type=Path,default=root/'Saved/Optimization06_validation/replay');a=p.parse_args()
 a.out.mkdir(parents=True,exist_ok=True)
 run=subprocess.run([str(a.exe)],capture_output=True,text=True,timeout=120)
 (a.out/'worker.log').write_text(run.stdout+run.stderr,encoding='utf-8')
 if run.returncode or 'PASS fixed-step exact' not in run.stdout:raise SystemExit('worker contract failed; see worker.log')
 rows=[]
 for line in run.stdout.splitlines():
  if line.startswith('repeat='):rows.append({k:float(v) for k,v in re.findall(r'(\w+)=([^ ]+)',line)})
 paths=[root/'Physics/PowderCore/CMakeLists.txt',*list((root/'Physics/PowderCore/include/powder').glob('*.hpp')),*list((root/'Physics/PowderCore/src').glob('*.cpp')),root/'Physics/PowderCore/tests/clump_worker_tests.cpp',root/'Source/VirtualGameTeamProj/PowderClumpTest.cpp',root/'Source/VirtualGameTeamProj/PowderClumpTest.h',root/'Source/VirtualGameTeamProj/DEMBridgeTests.cpp']
 report={'status':'PASS','exe_sha256':hashlib.sha256(a.exe.read_bytes()).hexdigest(),'source_sha256':{str(f.relative_to(root)).replace('\\','/'):hashlib.sha256(f.read_bytes()).hexdigest() for f in paths},'runs':rows,'physics_steps':12000,'physical_seconds':.024,'bodies':32,'agreement':'byte-identical body CSV and mixing files at fixed step count; history continuation identical','limitations':['32-body synthetic fixture, not kg-scale mm powder','worker is single-threaded physics; no contact parallelism','polling includes observation delay; timings are not a solver speedup claim','display/GPU cost not measured in standalone runner'],'contract':json.loads((root/'Physics/PowderCore/fixtures/optimization06_contract.json').read_text(encoding='utf-8'))}
 (a.out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print('PASS: '+str(a.out/'report.json'))
if __name__=='__main__':main()
