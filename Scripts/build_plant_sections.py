"""Independent source-triangle section fixtures; no OBB approximations."""
from pathlib import Path
import json,struct,math,csv,shutil
import numpy as np
ROOT=Path(__file__).resolve().parents[1];SRC=ROOT/'SourceAssets/ProcessPlantPhysics';OUT=ROOT/'SourceAssets/ProcessPlantSections';OUT.mkdir(exist_ok=True)
m=json.loads((SRC/'runtime_manifest.json').read_text(encoding='utf-8'));original=np.frombuffer((SRC/'runtime.plant').read_bytes(),dtype='<f4',offset=4).reshape(-1,4).copy()
def group(n):
 if n.startswith(('H-101','P101','V101','Collision_ASSUMED_Transition')):return 'supply'
 if n.startswith(('Collision_WH','CollisionPreview_ASSUMED_VT')):return 'weigh'
 if n.startswith(('P201','PL_','FH122','BV122')):return 'transfer'
 if n.startswith(('RH201','BV272')):return 'receiver'
 if n.startswith(('DP_','P202')):return 'drop'
 if n.startswith('MX-301'):return 'mixer'
 raise ValueError(n)
specs=[('SupplyToWeighing',{'supply','weigh'},['H-101','P101','V101','WH101','ASSUMED_WH','ASSUMED_VT'],[0,-140,260]),('WeighingToReceiver',{'weigh','transfer','receiver'},['WH101','ASSUMED_WH','ASSUMED_VT','P201','PL_','FH122','BV122','RH201','BV272','Test_PL_cuff_connection','Test_BV122_seal'],[120,0,180]),('ReceiverToMixer',{'receiver','drop','mixer'},['RH201','BV272','DP_','P202','MX-301','Test_BV272_seal'],[300,0,200]),('MixerOnly',{'mixer'},['MX-301'],[300,0,90])]
reports=[]
for section,(name,groups,prefixes,camera) in enumerate(specs,1):
 dst=OUT/name;dst.mkdir(exist_ok=True);packed=original.tolist();triangles=[];selected=[]
 for n in m['selected_meshes']:
  if group(n) not in groups:continue
  mesh=json.loads((SRC/(n+'.json')).read_text(encoding='utf-8'));v=np.array(mesh['vertices']);t=np.array(mesh['triangles'])
  if n=='Collision_WH101_Inspection_Lid':v+=np.array([.195,.195,1.837])
  triangles.extend(v[t]);selected.append(n)
 for sleeve in m['test_overrides']:
  if sleeve['kind']!='open_sleeve':continue
  keep=section==2 and sleeve['id']!='Test_BV272_seal' or section==3 and sleeve['id']=='Test_BV272_seal'
  if not keep:continue
  a=np.array(sleeve['a']);b=np.array(sleeve['b']);axis=b-a;axis/=np.linalg.norm(axis);u=np.cross(axis,[0,0,1] if abs(axis[2])<.9 else [0,1,0]);u/=np.linalg.norm(u);v=np.cross(axis,u)
  rings=[[p+r*(u*math.cos(j*math.tau/64)+v*math.sin(j*math.tau/64)) for j in range(64)] for p,r in [(a,sleeve['radius_a_m']),(b,sleeve['radius_b_m'])]]
  for j in range(64):
   k=(j+1)%64;triangles.extend([[rings[0][j],rings[1][j],rings[1][k]],[rings[0][j],rings[1][k],rings[0][k]]])
 ts=np.array(triangles);ts=ts[np.linalg.norm(np.cross(ts[:,1]-ts[:,0],ts[:,2]-ts[:,0]),axis=1)>1e-12]
 def bvh(ids):
  at=len(packed);packed.extend([[0.]*4,[0.]*4]);t=ts[ids];lo=t.min(axis=(0,1));hi=t.max(axis=(0,1))
  if len(ids)<=8:
   start=len(packed);packed.extend([list(v)+[0.] for tri in t for v in tri]);packed[at]=list(lo)+[-len(ids)];packed[at+1]=list(hi)+[start]
  else:
   c=t.mean(axis=1);axis=int(np.argmax(np.ptp(c,axis=0)));order=np.argsort(c[:,axis],kind='stable');mid=len(ids)//2;l=bvh(ids[order[:mid]]);r=bvh(ids[order[mid:]]);packed[at]=list(lo)+[l];packed[at+1]=list(hi)+[r]
  return at
 packed[0][0]=bvh(np.arange(len(ts)));packed[0][1]=original[0,1] if 'mixer' in groups else -1;packed[11]=[section,0,0,0];assert packed[10][0]==0
 # Compact to reachable BVHs and existing gas/gate data; remove unused source triangles.
 old=packed;packed=old[:16]
 def copytree(at):
  if at<0:return -1
  at=int(at);new=len(packed);lo=old[at].copy();hi=old[at+1].copy();packed.extend([lo,hi])
  if lo[3]>=0:lo[3]=copytree(lo[3]);hi[3]=copytree(hi[3])
  else:
   start=int(hi[3]);hi[3]=len(packed);packed.extend(old[start:start+int(-lo[3])*3])
  return new
 roots=[copytree(old[0][0]),copytree(old[0][1])]
 # Tail starts at original gate data; has no triangle pointers. Exclude appended BVH.
 begin=int(original[5,0]);end=len(original);shift=len(packed)-begin
 packed.extend([r.copy() for r in old[begin:end]])
 packed[0]=[roots[0],roots[1],float(original[0,2]+shift),float(original[0,3])];packed[5]=[float(original[5,0]+shift),float(original[5,1]),0,0];packed[6]=[float(original[6,0]+shift),*map(float,original[6,1:])]
 grid=int(original[6,0]);size=int(np.prod(original[6,1:]));
 for i in range(grid,grid+size):
  count=int(original[i,1])
  if count:
   at=int(original[i,0]);packed[i+shift][0]=at+shift
   for j in range(count):packed[at+j+shift][0]+=shift
 (dst/'runtime.plant').write_bytes(struct.pack('<I',len(packed))+np.array(packed,dtype='<f4').tobytes())
 routes=m['routes'];(dst/'runtime.routes').write_text(str(len(routes))+'\n'+'\n'.join(str(r['phase'])+' '+str(len(r['cells']))+' '+' '.join(str(c+shift) for c in r['cells']) for r in routes)+'\n')
 if section==1:shutil.copyfile(SRC/'test_charge.csv',dst/'test_charge.csv')
 else:
  centre={2:(0,0,1.2),3:(3,0,2.6),4:(2.7,0,.60)}[section]
  with (dst/'test_charge.csv').open('w',newline='') as f:
   w=csv.writer(f);w.writerow(['id','species','supplier','x_m','y_m','z_m','radius_m','density_kg_m3'])
   for i in range(3000):
    x=centre[0]+(i%20-9.5)*.003;y=centre[1]+(i//20%15-7)*.003;z=centre[2]+(i//300-4.5)*.003
    w.writerow([i,i//1000,'Section'+str(section),x,y,z,.00125,1200])
 report=dict(section=section,name=name,level='L_Section%02d_'%section+name,static_triangles=len(ts),rotor_triangles=m['rotor_triangles'] if 'mixer' in groups else 0,selected_meshes=selected,visual_prefixes=prefixes,camera_target_cm=camera,particle_count=3000,independent_initial_condition=True,bytes=(dst/'runtime.plant').stat().st_size)
 (dst/'manifest.json').write_text(json.dumps(report,indent=2));reports.append(report)
 print(name,len(ts),report['rotor_triangles'],report['bytes'])
(OUT/'sections.json').write_text(json.dumps(reports,indent=2))
