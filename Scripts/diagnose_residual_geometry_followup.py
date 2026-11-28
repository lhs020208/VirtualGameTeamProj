"""Read exported source geometry only; no source blend or UE asset writes."""
import json,csv,math
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
root=Path('D:/GitHub/VirtualGameTeamProj');out=root/'Docs/ProcessPlant_20261110_LowPoly/Followup_20261110';src=root/'Intermediate/PlantLowPoly20261110/Physics'
manifest=json.loads((root/'Docs/ProcessPlant_20261110_LowPoly/runtime_manifest.json').read_text());names=manifest['selected_meshes'];meshes=[]
for name in names:
 f=src/(name+'.json')
 if not f.exists():continue
 d=json.loads(f.read_text());v=d['vertices'];t=d['triangles']
 if name=='COLLISION_RH201_PowderPath_Static' and min(q[0] for q in v)<-.24:v=[[-q[0]+3,-q[1],q[2]+1.992] for q in v]
 lo=[min(q[k] for q in v) for k in range(3)];hi=[max(q[k] for q in v) for k in range(3)];meshes.append({'name':name,'v':v,'t':t,'lo':lo,'hi':hi,'tree':None})
results=[]
for group in ['B','WH','RH','B_before']:
 file=out/(group+('_input.csv' if group=='B_before' else '_residual_input.csv'))
 for row in csv.DictReader(file.open()):
  p=Vector([float(row[k]) for k in ['x_m','y_m','z_m']]);hits=[]
  for m in meshes:
   ds=sum(max(m['lo'][k]-p[k],0,p[k]-m['hi'][k])**2 for k in range(3))
   if ds>.01**2:continue
   if m['tree'] is None:m['tree']=BVHTree.FromPolygons(m['v'],m['t'],all_triangles=True)
   point,normal,index,distance=m['tree'].find_nearest(p,.01)
   if point is None:continue
   toward=(p-point).normalized();face=normal if normal.dot(toward)>=0 else -normal
   hits.append({'mesh':m['name'],'triangle':index,'distance_m':distance,'point_m':list(point),'toward_particle_normal':list(toward),'oriented_face_normal':list(face)})
  hits.sort(key=lambda v:v['distance_m']);results.append({'group':group,'id':int(row['id']),'position_m':list(p),'nearest':hits[:3]})
(out/'RESIDUAL_SOURCE_GEOMETRY.json').write_text(json.dumps(results,indent=2));print('geometry queries',len(results),'with nearest',sum(bool(r['nearest']) for r in results))
