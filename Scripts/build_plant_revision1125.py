"""Rebuild revised mixer boundaries; preserve the active charge and pressure routes."""
from pathlib import Path
import json, shutil, subprocess, sys
import numpy as np

root=Path(__file__).resolve().parents[1]
raw=root/'Intermediate/Plant1125/Physics'
hybrid=root/'Intermediate/Plant1125/Hybrid'
out=root/'SourceAssets/PlantRevision1125/Simulation'
doc=root/'Docs/PlantRevision1125'
def run(*args):subprocess.run([sys.executable,*map(str,args)],cwd=root,check=True)
run('Scripts/build_plant_runtime.py','--source',raw,'--mixer-pivot',3,0,.9)
shutil.copyfile(root/'SourceAssets/ProcessPlant_20261111_LowPoly/Simulation/test_charge.csv',raw/'test_charge.csv')
run('Scripts/build_plant_hybrid.py','--source',raw,'--output',hybrid,
    '--retain-mesh','MX-301_Collision_Vessel','--retain-mesh','MX-301_Collision_Rotor')
run('Scripts/build_plant_wall_index.py',hybrid,out)
manifest=json.loads((raw/'runtime_manifest.json').read_text(encoding='utf8'))
assert not any(n.startswith('ENV_') for n in manifest['selected_meshes'])
shutil.copyfile(raw/'runtime_manifest.json',doc/'runtime_source.json')
probes=[]
for rotor,name in enumerate(['MX-301_Collision_Vessel','MX-301_Collision_Rotor']):
    mesh=json.loads((raw/(name+'.json')).read_text(encoding='utf8'))
    triangles=np.asarray(mesh['vertices'])[np.asarray(mesh['triangles'])][::3]
    normal=np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0])
    length=np.linalg.norm(normal,axis=1);good=length>1e-10
    triangles=triangles[good];normal=normal[good]/length[good,None]
    for sign in [-1,1]:
        p=triangles.mean(axis=1)+sign*.001*normal
        probes.extend(np.column_stack([p,np.full(len(p),rotor)]).tolist())
np.asarray(probes,dtype='<f4').tofile(out/'mixer_surface_probes.bin')
print('REVISED_RUNTIME_READY',out,'mixer probes',len(probes))
