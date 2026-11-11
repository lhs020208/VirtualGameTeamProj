import json,math
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
root=Path('D:/GitHub/VirtualGameTeamProj');src=root/'Intermediate/PlantLowPoly20261110/Physics';out=root/'Docs/ProcessPlant_20261110_LowPoly/Followup_20261110';result={}
for name in ['MX-301_Collision_Vessel','P202_Reducer_272_to_44']:
 d=json.loads((src/(name+'.json')).read_text());tree=BVHTree.FromPolygons(d['vertices'],d['triangles'],all_triangles=True);records=[]
 for radius in [0,.008,.010,.011,.012,.0124,.0126,.014,.018,.020,.0219,.0221]:
  hits=[]
  for j in range(16):
   angle=math.tau*j/16;start=Vector((3+radius*math.cos(angle),radius*math.sin(angle),1.34));point,normal,tri,distance=tree.ray_cast(start,Vector((0,0,-1)),.5);hits.append({'z':point.z if point else None,'triangle':tri,'normal':list(normal) if normal else None})
  records.append({'ray_radius_m':radius,'hits':hits})
 result[name]=records
(out/'MIXER_INLET_AXIAL_RAYS.json').write_text(json.dumps(result,indent=2));print('ray tests written')
