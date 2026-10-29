"""Summarize fixed-physics rate validation, including failed-rate evidence."""
from pathlib import Path
import json,csv,statistics,shutil,hashlib
ROOT=Path(__file__).resolve().parents[1];base=ROOT/'Saved/SimpleVisualsRate';dest=ROOT/'Docs/DEM/simple_visuals_rate';dest.mkdir(parents=True,exist_ok=True);report={}
csv.field_size_limit(10000000)
for name in ['max30','target022_60','target020_30']:
 src=base/name
 if not (src/'summary.json').exists():continue
 item=json.loads((src/'summary.json').read_text());audit=src/'audit_report.json'
 if audit.exists():item['audit']=json.loads(audit.read_text())
 fps=[]
 for f in src.glob('Profile*.csv'):
  with f.open(encoding='utf-8-sig') as stream:
   rows=list(csv.DictReader(stream))
  start=2100 if name=='target020_30' else 150
  # For target020_30 the observed UI counter was 1978 after Running=true.
  # Start at 2100 to exclude paused frames; the console action was not logged as a CSV event.
  # Exclude deliberately paused startup from foreground validation.
  for i,r in enumerate(rows):
   if any(isinstance(v,str) and 'set PowderGranularMixer Running true' in v for v in r.values()):start=i+60
  dt=[]
  for r in rows[start:]:
   try:v=float(r.get('FrameTime',''))
   except (ValueError,TypeError):continue
   if v>0:dt.append(v)
  if dt:
   frame_dir=dest/name;frame_dir.mkdir(exist_ok=True)
   with (frame_dir/'frame_times_ms.csv').open('w',newline='') as stream:
    writer=csv.writer(stream);writer.writerow(['sample_index','frame_ms']);writer.writerows(enumerate(dt))
   sort=sorted(dt);fps.append({'file':f.name,'first_row_included':start,'frames':len(dt),'sample_wall_s':sum(dt)/1000,'average_fps':1000/statistics.mean(dt),'frame_ms_median':statistics.median(dt),'frame_ms_p95':sort[int(.95*(len(sort)-1))],'frame_ms_max':max(dt),'fraction_over_50ms':sum(v>50 for v in dt)/len(dt),'scope':'captured frames only; not all simulation wall time'})
 item['frame_samples']=fps;report[name]=item
 d=dest/name;d.mkdir(exist_ok=True)
 for f in ['summary.json','result.json','trace.csv','audit_report.json','timing.json','worker_timing.json','display_timing.json','monitor.json','command.json','material_model.json']:
  if (src/f).exists():shutil.copy2(src/f,d/f)
report['notes']={'target022_60':'Started behind the editor; brought foreground around t=18.4 physical seconds. Mixed-window run did not sustain target; do not quote full average as foreground capability.','target020_30':'Started paused, brought foreground, then Running=true via console. Active wall time excludes deliberate pause.','limits':['No empirical accuracy guarantee','No proof for all intervening contacts','Other resolutions, close-up camera or background workloads may change throughput','Saved map target and physics remain unchanged']}
report['asset_sha256']={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['Content/M_BulkFlow.uasset','Content/DEM/Tests/L_MX301_BinaryMaterials.umap','Content/DEM/Settings/DA_VirtualPowder_A.uasset','Content/DEM/Settings/DA_VirtualPowder_B.uasset']}
(dest/'summary.json').write_text(json.dumps(report,indent=2));print(json.dumps({k:{'rate':v['result']['rate'],'fps':v['frame_samples']} for k,v in report.items() if isinstance(v,dict) and 'result' in v},indent=2))
