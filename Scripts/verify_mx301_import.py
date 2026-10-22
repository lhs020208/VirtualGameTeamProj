import unreal as u,json
from pathlib import Path
report={}
for name in ['SM_MX301_TestVessel','SM_MX301_TestRotor']:
 mesh=u.load_asset('/Game/DEM/'+name);assert mesh
 b=mesh.get_bounds();report[name]={'origin_cm':[b.origin.x,b.origin.y,b.origin.z],'extent_cm':[b.box_extent.x,b.box_extent.y,b.box_extent.z]}
Path('D:/GitHub/VirtualGameTeamProj/Docs/DEM/optimization45/import_bounds.json').write_text(json.dumps(report,indent=2))
u.log('IMPORT_BOUNDS '+str(report))
