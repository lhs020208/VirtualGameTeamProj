from pathlib import Path
import subprocess,time,json,sys
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/(sys.argv[1] if len(sys.argv)>1 else 'Docs/DEM/coupled_minimum/followup_probe')
OUT.mkdir(exist_ok=True)
for batch in [1,4,8]:
 dest=OUT/f'batch{batch}'
 cmd=[str(ROOT/'Intermediate/PowderCore/Release/granular_coupled_probe.exe'),str(ROOT/'Saved/Rate01Recheck/current12/initial.bin'),str(dest),(sys.argv[2] if len(sys.argv)>2 else '512'),str(batch)]
 if len(sys.argv)>3:cmd.append(sys.argv[3])
 samples=[]
 with (OUT/f'batch{batch}.log').open('w') as log:
  p=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW);started=time.monotonic()
  while p.poll() is None:
   try:
    raw=subprocess.check_output(['nvidia-smi','--query-gpu=temperature.gpu,power.draw,utilization.gpu','--format=csv,noheader,nounits'],text=True,creationflags=subprocess.CREATE_NO_WINDOW,timeout=5).strip()
    temp,power,util=map(float,raw.split(','));samples.append({'elapsed':time.monotonic()-started,'temp':temp,'power':power,'util':util})
    if temp>=75 or power>=155: p.terminate();p.wait();raise RuntimeError('GPU temperature/power threshold reached')
   except subprocess.TimeoutExpired: p.terminate();p.wait();raise
   if time.monotonic()-started>180:p.terminate();p.wait();raise RuntimeError('probe timeout')
   time.sleep(.5)
  (OUT/f'batch{batch}_monitor.json').write_text(json.dumps(samples,indent=2),encoding='utf-8')
  if p.returncode:raise RuntimeError(f'probe {batch} failed; inspect log')
 print(dest,(dest/'result.json').read_text(),flush=True)
