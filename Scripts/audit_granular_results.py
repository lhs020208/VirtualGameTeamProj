from pathlib import Path
import json,csv,math,hashlib
import numpy as np
from scipy.spatial import cKDTree

ROOT=Path('D:/GitHub/VirtualGameTeamProj');OUT=ROOT/'Docs/DEM/optimization37_44'
def analyze(folder,n):
 p=np.fromfile(folder/'final.bin',dtype=np.float32).reshape(-1,4)
 assert len(p)==n and np.isfinite(p).all()
 assert np.isin(p[:,3],[0,1]).all()
 r=(.054/(n*4*math.pi/3))**(1/3)
 # Geometric intrusion against vessel/receiver enclosing faces (cut away in display only).
 main=p[:,2]>=0;receiver=p[:,2]<-.25
 wall=max(float(np.max(np.maximum(abs(p[main,0])-(.45-r),0),initial=0)),float(np.max(np.maximum(abs(p[main,1])-(.25-r),0),initial=0)),float(np.max(np.maximum(p[main,2]-(.4-r),0),initial=0)),float(np.max(np.maximum(-.6+r-p[receiver,2],0),initial=0)))
 return {'count':n,'radius_m':r,'groups':[int(np.sum(p[:,3]==k)) for k in (0,1)],'receiver_count':int(receiver.sum()),'mean_z':float(p[:,2].mean()),'observed_outer_wall_intrusion_m':wall,'finite':True}

report={'physical_accuracy_validated':False,'actual_mixer_asset_used':False,'reference_vessel_m':[.9,.5,.4],'nominal_bulk_litres':90,'solid_volume_m3':.054,'mass_kg':64.8,'cases':[],'renders':[]}
manifest=json.loads((OUT/'scale_manifest.json').read_text())
for item in manifest:
 label=item['label'];folder=OUT/label
 rows=list(csv.DictReader((folder/'trace.csv').open()))
 if item['status']!='completed':report['cases'].append(item);continue
 assert rows and all(math.isfinite(float(v)) for row in rows for v in row.values())
 assert all(int(row['count'])==item['count'] for row in rows)
 assert float(rows[-1]['time'])>=item['duration']-1e-5
 summary=dict(item);summary['last']=rows[-1];summary['max_observed_overlap_radius']=max(float(x['max_overlap_radius']) for x in rows)
 summary['snapshot']=analyze(folder,item['count'])
 if item['radius_override']:summary['snapshot']['radius_note']='audit outer-boundary tolerance used fixed-volume radius; fixed-radius case excluded from boundary acceptance';summary['mass_kg']=item['count']*4*math.pi/3*item['radius_override']**3*1200
 report['cases'].append(summary)
for folder in sorted((ROOT/'Saved').glob('Opt44_*final*')):
 result=json.loads((folder/'result.json').read_text(encoding='utf-8-sig'));assert result['error']==''
 result['snapshot']=analyze(folder,result['count']);result['label']=folder.name
 p=np.fromfile(folder/'final.bin',dtype=np.float32).reshape(-1,4);radius=result['snapshot']['radius_m']
 pairs=cKDTree(p[:,:3]).query_pairs(2*radius,output_type='ndarray')
 distances=np.linalg.norm(p[pairs[:,0],:3]-p[pairs[:,1],:3],axis=1) if len(pairs) else np.array([2*radius])
 result['final_snapshot_max_overlap_radius']=float(np.max((2*radius-distances)/radius))
 result['observed_geometry_gate']=result['final_snapshot_max_overlap_radius']<=.1 and result['snapshot']['observed_outer_wall_intrusion_m']<1e-4
 result['throughput_gate']=result['rate']>=.1
 result['test_protocol_completed']=result['time']>=12
 result['full_process_accepted']=False
 reference=OUT/('base30' if result['count']==30000 else 'n100')/'final.bin'
 if result['count'] in (30000,100000):
  labels=np.fromfile(reference,dtype=np.float32).reshape(-1,4)[:,3]
  assert np.array_equal(labels,p[:,3]), 'per-index group identity changed'
 result['group_identity_checked']=result['count'] in (30000,100000)

 assert result['time']>=12 and result['snapshot']['observed_outer_wall_intrusion_m']<1e-4
 report['renders'].append(result)
reference=next(c for c in report['cases'] if c['label']=='base30')['last']
report['refinement']={}
for label in ['dt_half','iter24']:
 last=next(c for c in report['cases'] if c['label']==label)['last']
 report['refinement'][label]={'mean_height_difference_m':abs(float(last['mean_z'])-float(reference['mean_z'])),'mix_difference':abs(float(last['mix_index'])-float(reference['mix_index']))}
report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'Physics/PowderCore/src/granular.cpp',ROOT/'Physics/PowderCore/src/granular_kernels.inc',ROOT/'Source/VirtualGameTeamProj/PowderGranularMixer.cpp']}
(OUT/'audit.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps({'refinement':report['refinement'],'render_rates':[(r['label'],r['rate']) for r in report['renders']]},indent=2))

