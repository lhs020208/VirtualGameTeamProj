import pathlib,subprocess,json,time
root=pathlib.Path(__file__).resolve().parents[1];out=root/'Docs/DemoScale200k'
exe=root/'Intermediate/Demo20261119/demo_scale_bench.exe'
for label,charge in [('core_20k',root/'SourceAssets/ProcessPlant_20261111_LowPoly/Simulation/test_charge.csv'),('core_200k',root/'Intermediate/DemoScale200k/charge_200k.csv')]:
 assert not (out/(label+'.json')).exists()
 args=[str(exe),str(root),str(charge),'90','180']
 with (out/(label+'.json')).open('w') as result,(out/(label+'.progress.txt')).open('w') as progress:
  p=subprocess.Popen(args,stdout=result,stderr=progress,cwd=root,creationflags=0x08000000)
  print('OWN_NATIVE',label,p.pid,flush=True);code=p.wait()
 if code:raise SystemExit(code)
 print((out/(label+'.json')).read_text(),flush=True)
