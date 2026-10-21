"""Benchmark accepted fixture-scoped candidates against the best unsoftened dt."""
from pathlib import Path
import argparse,hashlib,json,subprocess,statistics
from run_optimization08_validation import load,num
def main():
 root=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('results',type=Path);p.add_argument('new_output',type=Path);args=p.parse_args()
 report=json.loads((args.results/'report.json').read_text(encoding='utf-8'));exe=root/'Intermediate/PowderCore/Release/powder_soft08.exe'
 if report['status']!='completed' or hashlib.sha256(exe.read_bytes()).hexdigest()!=report['exe_sha256']:raise ValueError('completed matching executable required')
 args.new_output.mkdir(parents=True,exist_ok=False);c=report['contract'];results={}
 for case,cv in report['cases'].items():
  if not cv['accelerated_scoped_candidates']:continue
  selected=max(cv['accelerated_scoped_candidates'],key=lambda x:x[0]);baseline=[1,cv['best_unsoftened_dt_factor']];base=c['mm_baseline_dt_s'] if case=='mm_packing32' else c['baseline_dt_s'];times={'unsoftened':[],'softened':[]}
  for repeat in range(3):
   order=[('unsoftened',baseline),('softened',selected)]
   if repeat%2:order.reverse()
   for label,(alpha,factor) in order:
    dest=args.new_output/f'{case}_{label}_{repeat}';proc=subprocess.run([str(exe),case,str(alpha),format(base*factor,'.17g'),str(dest)],capture_output=True,text=True,timeout=240)
    if proc.returncode:raise RuntimeError(proc.stderr)
    reference=args.results/f"{case}_a{c['alphas'].index(alpha)}_d{c['dt_factors'].index(factor)}"
    for name in ['initial.csv','final.csv','states.csv','series.csv','events.csv','materials.csv']:
     if (dest/name).read_bytes()!=(reference/name).read_bytes():raise ValueError('physics repeat mismatch '+name)
    times[label].append(num(load(dest)['summary'],'wall_s'));print(proc.stdout.strip(),flush=True)
  results[case]={'alpha':selected[0],'dt_s':base*selected[1],'unsoftened_dt_s':base*baseline[1],'loop_seconds':times,'median_loop_speedup':statistics.median(times['unsoftened'])/statistics.median(times['softened']),'scope':'only this synthetic short fixture; CPU loop includes collection; not GPU/editor/kg-scale evidence'}
 (args.new_output/'benchmark.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
if __name__=='__main__':main()
