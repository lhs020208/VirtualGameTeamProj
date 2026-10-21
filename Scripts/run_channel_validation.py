"""Task 26 buffered planar channel supply, discharge and numerical verification."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
CMAKE=Path(r'C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')
def sources():
    paths=sorted(p for p in (ROOT/'Physics/FlowCore').rglob('*') if p.is_file() and p.suffix in ('.cpp','.hpp','.py','.txt','.flow','.channel') and '__pycache__' not in p.parts)
    paths.extend(ROOT/p for p in ('Physics/PowderCore/src/core.cpp','Physics/PowderCore/include/powder/core.hpp','Physics/PowderCore/tests/core_tests.cpp','Scripts/run_flow_contract_validation.py','Scripts/run_flow_solver_validation.py','Scripts/run_one_way_validation.py','Scripts/run_two_way_validation.py','Scripts/run_channel_validation.py'))
    return {p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    build=ROOT/'Intermediate/FlowCore';before=sources();started=time.perf_counter()
    report={'schema':1,'version':'0.5.0-buffered-channel','scope':'dilute_planar_virtual_particle_ports_inside_padded_gas_domain','started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_files':before,'source_sha256':hashlib.sha256(json.dumps(before,sort_keys=True).encode()).hexdigest(),'passed':False,'runs':[],'computed_runs':[]}
    def run(cmd,name):
        print('RUN',name,flush=True);t=time.perf_counter()
        with (out/name).open('w',encoding='utf-8') as f:p=subprocess.run([str(x) for x in cmd],cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,timeout=300,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        report['runs'].append({'command':list(map(str,cmd)),'log':name,'exit_code':p.returncode,'wall_seconds':time.perf_counter()-t})
        if p.returncode:raise RuntimeError(name+' failed')
    try:
        run([CMAKE,'-S',ROOT/'Physics/FlowCore','-B',build],'configure.log');run([CMAKE,'--build',build,'--config','Release'],'build.log')
        for repeat in (1,2):
            xml=out/f'tests_{repeat}.xml';run([CMAKE.with_name('ctest.exe'),'--test-dir',build,'-C','Release','--output-on-failure','--output-junit',xml],f'tests_{repeat}.log')
            tree=ET.parse(xml).getroot();assert int(tree.attrib['tests'])==11 and int(tree.attrib.get('failures',0))==0
            for kind in ('channel','two_way','one_way','flow_solver'):shutil.copyfile(build/f'{kind}_metrics.csv',out/f'{kind}_metrics_{repeat}.csv')
            target=out/f'channel_{repeat}';run([build/'Release/flow_channel_run.exe',ROOT/'Physics/FlowCore/fixtures/buffered_channel.flow',ROOT/'Physics/FlowCore/fixtures/buffered.channel',target],f'channel_{repeat}.log')
            data=json.loads((target/'result.json').read_text());assert data['passed'] and data['all_arrived'] and data['accepted_steps']==600 and data['exited']==3;report['computed_runs'].append(data)
        report['metrics_exact_repeat']=all((out/f'{kind}_metrics_1.csv').read_bytes()==(out/f'{kind}_metrics_2.csv').read_bytes() for kind in ('channel','two_way','one_way','flow_solver'))
        report['outputs_exact_repeat']=all((out/'channel_1'/name).read_bytes()==(out/'channel_2'/name).read_bytes() for name in ('input.flow','input.channel','history.csv','transits.csv','result.json'))
        report['sources_unchanged']=before==sources();report['passed']=all(report[k] for k in ('metrics_exact_repeat','outputs_exact_repeat','sources_unchanged'))
    except Exception as e:report['error']=str(e)
    report['wall_seconds']=time.perf_counter()-started
    (out/'report.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n',encoding='utf-8')
    (out/'report.md').write_text('# Task 26 buffered channel verification\n\nStatus: **'+('PASS' if report['passed'] else 'FAIL')+'**\n\n11 CTest suites twice; three scheduled particles over 0.3 physical seconds repeated exactly. FIFO supply, event residence times, gas/particle balances, kernel partitions, spatial/time refinement, independent filtered Poiseuille transit reference, partial completion and transactional failure checked.\n\nVirtual particle ports are interior to the gas domain. This is a dilute planar point-force baseline, not full-porosity CFD, dense powder, vacuum, real-equipment validation or Unreal visualization.\n\nSource SHA-256: `'+report['source_sha256']+'`\n',encoding='utf-8')
    print('PASS' if report['passed'] else 'FAIL',out);return 0 if report['passed'] else 1
if __name__=='__main__':sys.exit(main())
