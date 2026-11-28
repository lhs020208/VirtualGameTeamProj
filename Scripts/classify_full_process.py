from full_process_report_io import read_latest_report
"""Read-only snapshot postprocessing; never controls or resets the running UE test."""
import csv,json,sys,time,math
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
run=Path(sys.argv[1]);watch='--watch' in sys.argv
data=np.fromfile(ROOT/'SourceAssets/ProcessPlant_20261110_LowPoly/Simulation/runtime.plant',dtype='<f4',offset=4).reshape(-1,4)
gates=[data[k].copy() for k in range(int(data[5,0]),int(data[5,0])+2*int(data[5,1]),2) if data[k+1,0]==0]
base,count=int(data[0,2]),int(data[0,3]);air=data[base:base+3*count].reshape(count,3,4)
a,b=air[:,0,:3],air[:,1,:3];ab=b-a;length2=(ab*ab).sum(axis=1)
cache={}
for saved in (run/'classified').glob('*.json'):
 try:
  prior=json.loads(saved.read_text());cache[prior['snapshot']]=prior
 except (ValueError,KeyError,OSError):pass

def replace_retry(tmp,target):
 for attempt in range(30):
  try:tmp.replace(target);return True
  except PermissionError:time.sleep(.1)
 return False

def classify(file):
 rows=list(csv.DictReader(file.open(encoding='utf-8-sig')))
 pts=np.array([[float(r[k]) for k in ['x_m','y_m','z_m']] for r in rows]);species=np.array([int(float(r['species'])) for r in rows]);ids=np.array([int(r['id']) for r in rows])
 assert len(rows)==20000 and len(set(ids))==20000 and np.isfinite(pts).all()
 labels=np.full(len(rows),'Unclassified_not_proven_leak',dtype=object);nearest=np.full(len(rows),-1,dtype=int);clearance=np.full(len(rows),np.nan)
 x,y,z=pts.T
 for i,g in enumerate(gates):
  mask=(np.hypot(x-g[0],y-g[1])<.355)&(z>g[2]+.003)&(z<3.7);labels[mask]='Supply'+chr(65+i)
 def assign(name,mask):labels[(labels=='Unclassified_not_proven_leak')&mask]=name
 assign('WH',(np.hypot(x,y)<.355)&(z>.603)&(z<1.78))
 assign('WH_inlet_top',(np.hypot(x,y)<.355)&(z>=1.78)&(z<=1.8025))
 assign('RH',(np.hypot(x-3,y)<.255)&(z>2.163)&(z<3.7))
 assign('MX',(x>2.6225)&(x<3.3775)&(y>-.2265)&(y<.2265)&(z>.941)&(z<1.31))
 # Receiver discharge is gravity-driven and absent from the gas-route array.
 # Source-vertex-derived axisymmetric envelope, not exact signed-mesh containment.
 drop_radius=np.where(z>=2.133,.139,np.interp(z,[1.31,1.35,1.685,2.133],[.022,.022,.136,.136]))
 drop_r=np.hypot(x-3,y)
 drop=(z>=1.31)&(z<=2.163)&(drop_r<=drop_radius+.0025)
 assign('RH_to_MX_lumen_envelope',drop&(drop_r<=drop_radius))
 assign('RH_to_MX_boundary_tolerance',drop)
 unknown=np.where(labels=='Unclassified_not_proven_leak')[0]
 for start in range(0,len(unknown),256):
  ix=unknown[start:start+256];q=pts[ix,None,:]
  raw_t=((q-a)*ab).sum(axis=2)/np.maximum(length2,1e-20)
  t=np.clip(raw_t,0,1)
  delta=q-(a+raw_t[:,:,None]*ab);dist=np.linalg.norm(delta,axis=2)
  radius=air[:,0,3][None,:]+t*(air[:,1,3]-air[:,0,3])[None,:]
  radial=dist-radius
  # A clamped centreline plus radius is a capsule. Its large end-cap sphere
  # incorrectly labels space ABOVE the WH roof as an outlet pipe. Use flat caps.
  axial=np.maximum(-raw_t,raw_t-1)*np.sqrt(length2)
  gap=np.hypot(np.maximum(radial,0),np.maximum(axial,0))+np.minimum(np.maximum(radial,axial),0)
  best=gap.argmin(axis=1);v=gap[np.arange(len(ix)),best]
  nearest[ix]=base+3*best;clearance[ix]=v
  inside=v<=.0025
  labels[ix[inside]]=np.where(air[best[inside],2,3]==0,'Supply_to_WH_pipe','WH_to_RH_pipe')
 # Explicit conservative world envelope only. Not a leak claim for unresolved points.
 outside=(x< -2.5)|(x>4.5)|(y< -1.0)|(y>2.5)|(z<0)|(z>4.5)
 labels[(labels=='Unclassified_not_proven_leak')&outside]='Outside_conservative_plant_envelope'
 counts={name:[int(((labels==name)&(species==s)).sum()) for s in [0,1]] for name in sorted(set(labels))}
 assert [sum(v[s] for v in counts.values()) for s in [0,1]]==[10000,10000]
 dest=run/'classified';dest.mkdir(exist_ok=True)
 with (dest/file.name).open('w',newline='',encoding='utf8') as f:
  w=csv.writer(f);w.writerow(['sim_s','id','species','region','x_m','y_m','z_m','nearest_air_cell','approx_signed_pipe_distance_m'])
  for i,row in enumerate(rows):w.writerow([row['sim_s'],ids[i],species[i],labels[i],*pts[i],nearest[i],clearance[i]])
 summary={'snapshot':file.name,'sim_s':float(rows[0]['sim_s']),'counts':counts,'unknown_is_not_leak':True,'supply_gate_centres_m':[list(map(float,g[:3])) for g in gates]}
 (dest/(file.stem+'.json')).write_text(json.dumps(summary,indent=2));return summary
while True:
 status=read_latest_report(run)[0]
 for file in sorted(run.glob('[0-9]*.csv'),reverse=True):
  if file.name in cache:continue
  try:cache[file.name]=classify(file)
  except (ValueError,AssertionError,KeyError):continue # file may still be being written
 if cache:
  latest=cache[sorted(cache)[-1]];result={'source_status_latest_sim_s':status.get('latest',{}).get('sim_s'),'classification':'supply includes cylinder AND cone above actual gate; pipes use gas-path volumes; unknown is not proven leak','latest':latest,'snapshots_processed':len(cache)}
  tmp=run/'classified_status.tmp';tmp.write_text(json.dumps(result,indent=2));replace_retry(tmp,run/'classified_status.json')
  names=sorted({name for r in cache.values() for name in r['counts']})
  with (run/'classified_progress.csv').open('w',newline='') as f:
   w=csv.writer(f);w.writerow(['snapshot','sim_s']+[n+'_'+s for n in names for s in ['A','B']])
   for key in sorted(cache):r=cache[key];w.writerow([key,r['sim_s']]+[v for n in names for v in r['counts'].get(n,[0,0])])
 if not watch or 'finished_utc' in status:break
 time.sleep(3)
print(json.dumps(result))
