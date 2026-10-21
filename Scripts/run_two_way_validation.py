"""Task 25 dilute point-force two-way exchange verification."""
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
    paths=sorted(p for p in (ROOT/'Physics/FlowCore').rglob('*') if p.is_file() and p.suffix in ('.cpp','.hpp','.py','.txt','.flow') and '__pycache__' not in p.parts)
    paths.extend(ROOT/path for path in ('Physics/PowderCore/src/core.cpp','Physics/PowderCore/include/powder/core.hpp','Physics/PowderCore/tests/core_tests.cpp','Scripts/run_flow_contract_validation.py','Scripts/run_flow_solver_validation.py','Scripts/run_one_way_validation.py','Scripts/run_two_way_validation.py'))
    return {p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False);build=ROOT/'Intermediate/FlowCore';before=sources()
    report={'schema':1,'version':'0.4.0-dilute-two-way','scope':'conservative_volume_diagnostics_and_dilute_momentum_feedback','finite_porosity_equations':False,'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_files':before,'source_sha256':hashlib.sha256(json.dumps(before,sort_keys=True).encode()).hexdigest(),'passed':False,'runs':[],'computed_runs':[]}
    def run(cmd,name):
        print('RUN',name,flush=True)
        with (out/name).open('w',encoding='utf-8') as f:r=subprocess.run([str(x) for x in cmd],cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,timeout=300,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        report['runs'].append({'command':[str(x) for x in cmd],'log':name,'exit_code':r.returncode})
        if r.returncode:raise RuntimeError(f'{name} failed')
    try:
        run([CMAKE,'-S',ROOT/'Physics/FlowCore','-B',build],'configure.log');run([CMAKE,'--build',build,'--config','Release'],'build.log')
        for repeat in (1,2):
            xml=out/f'tests_{repeat}.xml';run([CMAKE.with_name('ctest.exe'),'--test-dir',build,'-C','Release','-E','^flow_channel','--output-on-failure','--output-junit',xml],f'tests_{repeat}.log')
            tree=ET.parse(xml).getroot()
            if int(tree.attrib['tests'])!=9 or int(tree.attrib.get('failures',0)):raise RuntimeError('unexpected CTest result')
            for kind in ('two_way','one_way','flow_solver'):(out/f'{kind}_metrics_{repeat}.csv').write_bytes((build/f'{kind}_metrics.csv').read_bytes())
            for mode,flag in (('two_way',1),('control',0)):
                target=out/f'{mode}_{repeat}'
                run([build/'Release/flow_two_way_run.exe',ROOT/'Physics/FlowCore/fixtures/two_way_channel.flow',target,20,flag],f'{mode}_{repeat}.log')
                result=json.loads((target/'result.json').read_text())
                if not result['passed'] or result['accepted_steps']!=20 or result['reaction_enabled']!=bool(flag):raise RuntimeError('incomplete run')
                report['computed_runs'].append(result)
        report['metrics_exact_repeat']=all((out/f'{kind}_metrics_1.csv').read_bytes()==(out/f'{kind}_metrics_2.csv').read_bytes() for kind in ('two_way','one_way','flow_solver'))
        report['results_exact_repeat']=all((out/f'{mode}_1'/name).read_bytes()==(out/f'{mode}_2'/name).read_bytes() for mode in ('two_way','control') for name in ('result.json','exchange.csv','volume_pressure.csv','air_velocity.csv','input.flow','coupling.json'))
        report['sources_unchanged']=before==sources()
        report['passed']=all(report[k] for k in ('metrics_exact_repeat','results_exact_repeat','sources_unchanged'))
    except Exception as e:report['error']=str(e)
    (out/'report.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n',encoding='utf-8')
    (out/'report.md').write_text('# Task 25 dilute two-way verification\n\nStatus: **'+('PASS' if report['passed'] else 'FAIL')+'**\n\nNine CTest suites twice: conservative volume assignment, equal/opposite actual impulse injection, adjoint power identity, exchange decay reference, fixed-support grid and coupling-time refinement, off-node and overlapping stencils, weak one-way limit, dense/input rejection, atomic rollback and previous solver regressions. Two-way and matched one-way 0.01 s runs repeated byte-for-byte.\n\nVolume fractions are diagnostics/guards (alpha<=0.001). Finite-porosity continuity, dense drag, self-disturbance correction, contacts and Unreal integration are not implemented here.\n\nSource SHA-256: `'+report['source_sha256']+'`\n',encoding='utf-8')
    print('PASS' if report['passed'] else 'FAIL',out);return 0 if report['passed'] else 1
if __name__=='__main__':sys.exit(main())
