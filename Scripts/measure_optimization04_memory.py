"""Measure process peak working set separately from solver timing comparisons."""
import argparse,json,subprocess,time
from pathlib import Path
from run_optimization01_validation import memory_reader
ROOT=Path(__file__).resolve().parents[1]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True);ap.add_argument('--reference-exe',type=Path,required=True);a=ap.parse_args();out=a.output.resolve();out.mkdir(exist_ok=False,parents=True);results=[]
 for name,exe,mode in [('optimization03',a.reference_exe.resolve(),'grid'),('optimization04',ROOT/'Intermediate/PowderCore/Release/powder_opt01.exe','reuse')]:
  with (out/(name+'.log')).open('w',encoding='utf-8') as f:
   p=subprocess.Popen([str(exe),str(ROOT/'Physics/PowderCore/fixtures/optimization01.bench'),str(out/name),'0',mode],stdout=f,stderr=subprocess.STDOUT,cwd=ROOT,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0));sample,close=memory_reader(p.pid);peak=0
   try:
    while p.poll() is None:
     if sample:peak=max(peak,sample() or 0)
     time.sleep(.005)
   finally:
    if close:close()
   if p.returncode:raise RuntimeError(name+' failed')
  results.append({'mode':name,'process_peak_working_set_bytes':peak or None,'scope':'entire benchmark process, not isolated solver buffers; separate run from timing trials'})
 (out/'memory.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8');print(json.dumps(results,indent=2))
if __name__=='__main__':main()
