"""Task 24: verify restricted air-to-particle coupling, never two-way CFD-DEM."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
CMAKE=Path(r'C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')
def sources():
    files=sorted(p for p in (ROOT/'Physics/FlowCore').rglob('*') if p.is_file() and p.suffix in ('.cpp','.hpp','.py','.txt','.flow') and '__pycache__' not in p.parts)
    files.extend(ROOT/path for path in ('Physics/PowderCore/src/core.cpp','Physics/PowderCore/include/powder/core.hpp','Physics/PowderCore/tests/core_tests.cpp','Scripts/run_flow_contract_validation.py','Scripts/run_flow_solver_validation.py','Scripts/run_one_way_validation.py'))
    return {p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False);build=ROOT/'Intermediate/FlowCore';before=sources()
    report={'schema':1,'version':'0.3.0-one-way','scope':'isolated_heavy_spheres_Stokes_air_to_particle_only','started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_files':before,'source_sha256':hashlib.sha256(json.dumps(before,sort_keys=True).encode()).hexdigest(),'passed':False,'runs':[],'computed_runs':[]}
    def run(cmd,name):
        print('RUN',name,flush=True)
        with (out/name).open('w',encoding='utf-8') as f:
            result=subprocess.run([str(x) for x in cmd],cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,timeout=300,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        report['runs'].append({'command':[str(x) for x in cmd],'log':name,'exit_code':result.returncode})
        if result.returncode:raise RuntimeError(f'{name} failed')
    try:
        run([CMAKE,'-S',ROOT/'Physics/FlowCore','-B',build],'configure.log');run([CMAKE,'--build',build,'--config','Release'],'build.log')
        for repeat in (1,2):
            xml=out/f'tests_{repeat}.xml';run([CMAKE.with_name('ctest.exe'),'--test-dir',build,'-C','Release','-E','^(flow_two_way|flow_channel)','--output-on-failure','--output-junit',xml],f'tests_{repeat}.log')
            tree=ET.parse(xml).getroot()
            if int(tree.attrib['tests'])!=7 or int(tree.attrib.get('failures',0)):raise RuntimeError('unexpected CTest result')
            for kind in ('one_way','flow_solver'):(out/f'{kind}_metrics_{repeat}.csv').write_bytes((build/f'{kind}_metrics.csv').read_bytes())
            run([build/'Release/flow_particle_run.exe',ROOT/'Physics/FlowCore/fixtures/one_way_channel.flow',out/f'coupled_{repeat}'],f'coupled_{repeat}.log')
            result=json.loads((out/f'coupled_{repeat}/result.json').read_text())
            if not result['passed'] or result['air_reaction_enabled'] or result['accepted_air_steps']!=100:raise RuntimeError('invalid computed result')
            report['computed_runs'].append(result)
        report['metrics_exact_repeat']=all((out/f'{kind}_metrics_1.csv').read_bytes()==(out/f'{kind}_metrics_2.csv').read_bytes() for kind in ('one_way','flow_solver'))
        report['trajectory_exact_repeat']=all((out/'coupled_1'/name).read_bytes()==(out/'coupled_2'/name).read_bytes() for name in ('result.json','particles.csv','particle.json','input.flow'))
        report['sources_unchanged']=before==sources()
        report['passed']=all(report[k] for k in ('metrics_exact_repeat','trajectory_exact_repeat','sources_unchanged'))
    except Exception as e:report['error']=str(e)
    (out/'report.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n',encoding='utf-8')
    (out/'report.md').write_text('# Task 24 one-way verification\n\nStatus: **'+('PASS' if report['passed'] else 'FAIL')+'**\n\nSeven CTest suites run twice, including prior air and reused DEM translation checks. Affine and grid-convergent MAC sampling, Stokes relaxation, settling, particle/coupled time refinement, impulse balance, one-way invariance, exact repeat, scope rejection and rollback tested. Computed 0.05 s particle trajectories repeated exactly.\n\nThis is not two-way CFD-DEM, dense powder, wall-contact, real-vacuum or Unreal verification.\n\nSource SHA-256: `'+report['source_sha256']+'`\n',encoding='utf-8')
    print('PASS' if report['passed'] else 'FAIL',out);return 0 if report['passed'] else 1
if __name__=='__main__':sys.exit(main())
