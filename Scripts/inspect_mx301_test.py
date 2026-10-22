import bpy,json
from pathlib import Path
data=[]
for o in bpy.context.scene.objects:
 if o.type=='MESH' and o.name.startswith('MX-301'):
  data.append({'name':o.name,'dimensions':list(o.dimensions),'location':list(o.matrix_world.translation)})
Path('D:/GitHub/VirtualGameTeamProj/Docs/DEM/optimization45/model_objects.json').write_text(json.dumps(data,indent=2))
