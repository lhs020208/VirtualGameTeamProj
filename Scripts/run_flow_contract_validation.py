"""Task 22: standalone contract validation, not CFD solver validation."""
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
    files.append(Path(__file__).resolve())
    return {p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
def save(p,value):p.write_text(json.dumps(value,indent=2,allow_nan=False)+'\n',encoding='utf-8')
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False);build=ROOT/'Intermediate/FlowCore';before=sources()
    report={'schema':1,'scope':'input_contract_only_no_flow_solver','started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_files':before,'source_sha256':hashlib.sha256(json.dumps(before,sort_keys=True).encode()).hexdigest(),'passed':False,'runs':[]}
    def run(cmd,name):
        print('RUN',name,flush=True)
        with (out/name).open('w',encoding='utf-8') as f:r=subprocess.run([str(x) for x in cmd],cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,timeout=300,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        report['runs'].append({'command':[str(x) for x in cmd],'log':name,'exit_code':r.returncode})
        if r.returncode:raise RuntimeError(f'{name} failed')
    try:
        run([CMAKE,'-S',ROOT/'Physics/FlowCore','-B',build],'configure.log');run([CMAKE,'--build',build,'--config','Release'],'build.log')
        for repeat in (1,2):
            xml=out/f'tests_{repeat}.xml';run([CMAKE.with_name('ctest.exe'),'--test-dir',build,'-C','Release','-R','^flow_contract','--output-on-failure','--output-junit',xml],f'tests_{repeat}.log')
            tree=ET.parse(xml).getroot();assert int(tree.attrib['tests'])==2 and int(tree.attrib.get('failures',0))==0
            for name in ('pressure_channel','velocity_channel','zero_drive'):
                run([build/'Release/flow_contract.exe',ROOT/'Physics/FlowCore/fixtures'/(name+'.flow'),out/f'{name}_{repeat}.json'],f'{name}_{repeat}.log')
        report['exact_repeat']=all((out/f'{name}_1.json').read_bytes()==(out/f'{name}_2.json').read_bytes() for name in ('pressure_channel','velocity_channel','zero_drive'))
        report['sources_unchanged']=before==sources();report['passed']=report['exact_repeat'] and report['sources_unchanged']
    except Exception as e:report['error']=str(e)
    save(out/'report.json',report)
    (out/'report.md').write_text('# FlowCore contract validation\n\nStatus: **'+('PASS' if report['passed'] else 'FAIL')+'**\n\nScope: task 22 input contracts, units, supported regime, MAC data layout, analytical references and serialization. This contract-only run does not validate the flow solver.\n\nTwo CTest suites (C++ and CLI) run twice; three valid input reports compared byte-for-byte.\n\nSource SHA-256: `'+report['source_sha256']+'`\n',encoding='utf-8')
    print('PASS' if report['passed'] else 'FAIL',out);return 0 if report['passed'] else 1
if __name__=='__main__':sys.exit(main())
