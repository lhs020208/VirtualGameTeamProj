"""Fit virtual friction to supplied measurements; never invent missing targets.
Input CSV: split,metric,time_s,value,sigma
Metrics match this exact reference rig: receiver_kg, mean_z_m, mix_index.
Use distinct measured conditions for independent physical validation; this script
only screens a candidate grid for the supplied reference-rig observations.
"""
import csv,json,math,sys
from pathlib import Path
root=Path('D:/GitHub/VirtualGameTeamProj/Docs/DEM/optimization45')
measurements=Path(sys.argv[1]) if len(sys.argv)>1 else root/'material_measurements.csv'
output=root/'material_calibration.json'
rows=list(csv.DictReader(measurements.open(encoding='utf-8-sig'))) if measurements.exists() else []
if not rows:
 output.write_text(json.dumps({'status':'awaiting_measured_data','physical_accuracy_validated':False,'reason':'No measured reference-rig observations supplied; friction remains virtual.'},indent=2));print('awaiting_measured_data');sys.exit(0)
allowed={'receiver_kg','mean_z_m','mix_index'}
for row in rows:
 assert row['split'] in ('fit','holdout') and row['metric'] in allowed
 assert all(math.isfinite(float(row[k])) for k in ('time_s','value','sigma')) and float(row['sigma'])>0
assert any(r['split']=='fit' for r in rows)
candidates=[]
for friction,label in [(.2,'friction02'),(.4,'reference12'),(.6,'friction06')]:
 sim=list(csv.DictReader((root/label/'trace.csv').open()))
 errors=[]
 for row in rows:
  t=float(row['time_s']);assert float(sim[0]['time'])<=t<=float(sim[-1]['time'])
  def value(x):return float(x['receiver_count'])*64.8/int(x['count']) if row['metric']=='receiver_kg' else float(x['mean_z'] if row['metric']=='mean_z_m' else x['mix_index'])
  i=next(i for i,x in enumerate(sim) if float(x['time'])>=t);a=sim[max(0,i-1)];b=sim[i];span=float(b['time'])-float(a['time']);u=(t-float(a['time']))/span if span else 0
  prediction=value(a)*(1-u)+value(b)*u
  errors.append(dict(split=row['split'],metric=row['metric'],time_s=t,prediction=prediction,normalized_error=(prediction-float(row['value']))/float(row['sigma'])))
 fit=[e['normalized_error']**2 for e in errors if e['split']=='fit']
 candidates.append(dict(friction=friction,fit_mean_square=sum(fit)/len(fit),errors=errors))
best=min(candidates,key=lambda x:x['fit_mean_square']);holdout=[abs(e['normalized_error']) for e in best['errors'] if e['split']=='holdout']
report=dict(status='candidate_fit_requires_review',selected_friction=best['friction'],holdout_within_3sigma=bool(holdout) and max(holdout)<=3,physical_accuracy_validated=False,candidates=candidates)
output.write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
