import csv,json,subprocess,sys,tempfile
from pathlib import Path
exe=Path(sys.argv[1])
def run(*args):return subprocess.run([str(exe),*map(str,args)],capture_output=True,text=True,timeout=30)
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);f=p/'input.bench';text='POWDER_OPT01 1\ncase tiny freefall 8 .02 2500 10000 8 2500 2 .4 .000002 20 10 0 pass\nend\n';f.write_text(text)
 for flag in (0,1):
  result=run(f,p/str(flag),flag);assert result.returncode==0,result.stderr;assert json.loads((p/str(flag)/'result.json').read_text())['passed']
 for name in ('physics.csv','tiny_initial.csv','tiny_final.csv','tiny_events.csv'):
  assert (p/'0'/name).read_bytes()==(p/'1'/name).read_bytes()
 old=(p/'0'/'result.json').read_bytes();assert run(f,p/'0',0).returncode==2 and (p/'0'/'result.json').read_bytes()==old
 assert run(f,p/'bad',2).returncode==2 and not (p/'bad').exists()
 f.write_text(text+'junk');assert run(f,p/'junk',0).returncode==2 and not (p/'junk').exists()
 f.write_text(text.replace('.000002','1'));assert run(f,p/'failed',1).returncode==3
 row=list(csv.DictReader((p/'failed'/'physics.csv').open()))[0];assert row['status']=='failed' and int(row['accepted_steps'])==0
 assert (p/'failed'/'tiny_initial.csv').read_bytes()==(p/'failed'/'tiny_final.csv').read_bytes()
print('PASS optimization CLI formats, nonoverwrite, invalid input, exact profile parity, last accepted state')
