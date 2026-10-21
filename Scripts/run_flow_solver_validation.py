"""Build and verify the restricted air-only FlowCore solver, not CFD-DEM."""
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
    files.extend([Path(__file__).resolve(),ROOT/'Scripts/run_flow_contract_validation.py'])
    return {p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    build=ROOT/'Intermediate/FlowCore';before=sources()
    report={'schema':1,'scope':'restricted_2D_air_only_solver_no_DEM_coupling','flow_core_version':'0.2.0','started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_files':before,'source_sha256':hashlib.sha256(json.dumps(before,sort_keys=True).encode()).hexdigest(),'passed':False,'runs':[],'fixtures':{}}
    def run(cmd,name):
        print('RUN',name,flush=True)
        with (out/name).open('w',encoding='utf-8') as f:
            r=subprocess.run([str(x) for x in cmd],cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,timeout=300,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        report['runs'].append({'command':[str(x) for x in cmd],'log':name,'exit_code':r.returncode})
        if r.returncode:raise RuntimeError(f'{name} failed; inspect log and last accepted state')
    try:
        run([CMAKE,'-S',ROOT/'Physics/FlowCore','-B',build],'configure.log')
        run([CMAKE,'--build',build,'--config','Release'],'build.log')
        for repeat in (1,2):
            xml=out/f'tests_{repeat}.xml'
            run([CMAKE.with_name('ctest.exe'),'--test-dir',build,'-C','Release','-R','^(flow_solver|flow_contract|flow_run_cli|flow_contract_cli)$','--output-on-failure','--output-junit',xml],f'tests_{repeat}.log')
            tree=ET.parse(xml).getroot()
            if int(tree.attrib['tests'])!=4 or int(tree.attrib.get('failures',0))!=0:raise RuntimeError('unexpected CTest result')
            (out/f'numerical_metrics_{repeat}.csv').write_bytes((build/'flow_solver_metrics.csv').read_bytes())
            for name in ('pressure_channel','velocity_channel','zero_drive'):
                target=out/f'{name}_{repeat}'
                run([build/'Release/flow_run.exe',ROOT/'Physics/FlowCore/fixtures'/(name+'.flow'),target],f'{name}_{repeat}.log')
                result=json.loads((target/'result.json').read_text())
                if not result['passed'] or result['accepted_steps']!=10000 or result['physical_time_s']!=5:raise RuntimeError('incomplete fixture evolution')
                report['fixtures'][f'{name}_{repeat}']=result
        report['numerical_metrics_exact_repeat']=(out/'numerical_metrics_1.csv').read_bytes()==(out/'numerical_metrics_2.csv').read_bytes()
        report['computed_fields_exact_repeat']=all((out/f'{name}_1'/file).read_bytes()==(out/f'{name}_2'/file).read_bytes() for name in ('pressure_channel','velocity_channel','zero_drive') for file in ('last_accepted_fields.csv','history.csv','input.flow','contract.json'))
        report['sources_unchanged']=before==sources()
        report['passed']=all(report[k] for k in ('numerical_metrics_exact_repeat','computed_fields_exact_repeat','sources_unchanged'))
    except Exception as e:
        report['error']=str(e)
    (out/'report.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n',encoding='utf-8')
    (out/'report.md').write_text('# Task 23 air-flow verification\n\nStatus: **'+('PASS' if report['passed'] else 'FAIL')+'**\n\nFour CTest suites run twice. Numerical evidence includes steady channel grid convergence, 2D manufactured Poisson solutions, startup series and Euler time refinement, continuity, pressure/velocity boundaries, 2D perturbation, repeatability and transactional failure. Three complete 5 s fixtures run twice.\n\nScope: near-atmospheric 2D planar isothermal laminar air only. No particle coupling, real vacuum pipe, turbulence, or Unreal visualization validation.\n\nSource SHA-256: `'+report['source_sha256']+'`\n',encoding='utf-8')
    print('PASS' if report['passed'] else 'FAIL',out)
    return 0 if report['passed'] else 1
if __name__=='__main__':sys.exit(main())
