"""CPU parallel parity, deterministic ownership and thread/memory measurements."""
import argparse, csv, hashlib, json, platform, statistics, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True);ap.add_argument('--reference-exe',type=Path,required=True);a=ap.parse_args()
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);exe=ROOT/'Intermediate/PowderCore/Release/parallel_tests.exe'
 contract={'threads':[1,2,4,8],'repeats':3,'warmup_steps':100,'measured_steps':2000,'dt_s':1e-8,'counts':[32,128,256],'clumps':[0,1],'default_threads':1,'default_min_jobs':128,'physics_tolerance':'bit-exact same-build trajectory, history continuation and reports; momentum absolute < 1e-12 kg m/s','scope':'pair force evaluation and rigid integration; serial deterministic force reduction, wall geometry/contact and post guards','race_detector':'not run; disjoint output ownership and stress/barrier tests','large_scale_target_achieved':False}
 (out/'contract.json').write_text(json.dumps(contract,indent=2),encoding='utf-8')
 result=subprocess.run([str(exe)],capture_output=True,text=True,check=True);(out/'parallel_tests.log').write_text(result.stdout,encoding='utf-8')
 samples=[]
 contract['clumps']=[0,1,2,3]
 contract['fixture_codes']={'0':'sphere contact network','1':'two-pebble clump contact network','2':'sphere contacts plus static/moving mesh walls','3':'freefall without contacts'}
 (out/'contract.json').write_text(json.dumps(contract,indent=2),encoding='utf-8')
 for repeat in range(3):
  for count in contract['counts']:
   for clumps in contract['clumps']:
    for threads in (contract['threads'] if repeat%2==0 else list(reversed(contract['threads']))):
     print('BENCH',repeat,count,clumps,threads,flush=True)
     row=json.loads(subprocess.check_output([str(exe),str(count),str(threads),str(clumps),'2000'],text=True));row['repeat']=repeat;samples.append(row)
 (out/'samples.json').write_text(json.dumps(samples,indent=2),encoding='utf-8')
 summaries=[]
 for count in contract['counts']:
  for clumps in contract['clumps']:
   base=statistics.median(r['wall_s'] for r in samples if r['count']==count and r['clumps']==clumps and r['threads']==1)
   for threads in contract['threads']:
    rows=[r for r in samples if r['count']==count and r['clumps']==clumps and r['threads']==threads]
    med=statistics.median(r['wall_s'] for r in rows)
    summaries.append({'count':count,'clumps':clumps,'threads':threads,'median_wall_s':med,'speedup':base/med,'workspace_bytes':max(r['workspace_bytes'] for r in rows),'max_working_set_bytes':max(r['working_set_bytes'] for r in rows),'max_private_bytes':max(r['private_bytes'] for r in rows)})
 # Before/current default physics, including expected rejection cases and flow/mixing data.
 for name,program in [('before',a.reference_exe),('default',ROOT/'Intermediate/PowderCore/Release/powder_opt01.exe')]:
  p=subprocess.run([str(program),str(ROOT/'Physics/PowderCore/fixtures/optimization01.bench'),str(out/name),'1','walls'],capture_output=True,text=True,check=True)
  (out/(name+'.log')).write_text(p.stdout,encoding='utf-8')
 files=[p.name for p in (out/'before').glob('*.csv') if p.name=='physics.csv' or p.name.endswith(('_initial.csv','_final.csv','_events.csv'))]
 for name in files:assert (out/'before'/name).read_bytes()==(out/'default'/name).read_bytes(),name
 historical=[]
 for repeat in range(3):
  for threads in (contract['threads'] if repeat%2==0 else list(reversed(contract['threads']))):
   folder=out/f'historical_{repeat}_{threads}';print('HISTORICAL',repeat,threads,flush=True)
   subprocess.run([str(ROOT/'Intermediate/PowderCore/Release/powder_opt01.exe'),str(ROOT/'Physics/PowderCore/fixtures/optimization01.bench'),str(folder),'1','walls',str(threads),'1'],capture_output=True,text=True,check=True)
   for name in files:assert (out/'before'/name).read_bytes()==(folder/name).read_bytes(),(folder,name)
   with (folder/'timings.csv').open() as f:
    for row in csv.DictReader(f):row.update(repeat=repeat,threads=threads);historical.append(row)
 historical_summary=[]
 for case in sorted({r['case'] for r in historical}):
  base=statistics.median(float(r['loop_s']) for r in historical if r['case']==case and r['threads']==1)
  for threads in contract['threads']:
   rows=[r for r in historical if r['case']==case and r['threads']==threads];med=statistics.median(float(r['loop_s']) for r in rows)
   historical_summary.append({'case':case,'threads':threads,'median_loop_s':med,'speedup':base/med,'median_rate':statistics.median(float(r['loop_rate']) for r in rows)})
 sources={str(p.relative_to(ROOT)):sha(p) for folder in ['Physics/PowderCore/include','Physics/PowderCore/src','Physics/PowderCore/tests','Source/VirtualGameTeamProj'] for p in (ROOT/folder).rglob('*') if p.suffix in ['.cpp','.hpp','.h']}
 report={'passed':True,'contract':contract,'machine':platform.platform(),'executable_sha256':sha(exe),'reference_sha256':sha(a.reference_exe),'default_exact_csv_files':len(files),'parallel_historical_exact_csv_comparisons':len(files)*12,'summaries':summaries,'historical_summary':historical_summary,'source_sha256':sources}
 (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
 print(json.dumps(summaries,indent=2),flush=True)
if __name__=='__main__':main()
