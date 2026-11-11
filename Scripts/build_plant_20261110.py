"""Rebuild only the independent low-poly runtime after export_plant_20261110.py."""
import json,shutil,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
src=root/'Intermediate/PlantLowPoly20261110/Physics'
hybrid=root/'Intermediate/PlantLowPoly20261110/Hybrid'
out=root/'SourceAssets/ProcessPlant_20261110_LowPoly/Simulation'
docs=root/'Docs/ProcessPlant_20261110_LowPoly'
m=json.loads((src/'manifest.json').read_text(encoding='utf8'))
assert m['sha256']=='93d21f408c7c4d0f2e22252acfb9b285bf6ba5a72922d63e8c4ebdfd52e84034'
for s in m['suppliers']:
 s['particle_count']=10000 if s['id'] in ['H-101A','H-101B'] else 0
 s['radius_m']=.002
(src/'manifest.json').write_text(json.dumps(m,ensure_ascii=False,indent=2),encoding='utf8')
def run(script,*args):subprocess.run([sys.executable,str(root/'Scripts'/script),*map(str,args)],cwd=root,check=True)
run('prepare_plant_particle_charge.py',src)
run('build_plant_runtime.py','--source',src,'--mixer-pivot',3,0,1.105)
# These intersecting inlet/roof and paddle/shaft contacts are deliberately exact meshes.
run('build_plant_hybrid.py','--source',src,'--output',hybrid,'--retain-mesh','MX-301_Collision_Vessel','--retain-mesh','MX-301_Collision_Rotor')
run('build_plant_wall_index.py',hybrid,out)
shutil.copy2(src/'test_charge_report.json',out/'test_charge_report.json')
shutil.copy2(src/'manifest.json',docs/'physics_source.json')
shutil.copy2(src/'runtime_manifest.json',docs/'runtime_manifest.json')
# Preserve pre-fallback surface queries, including the complex mixer junctions.
shutil.copy2(docs/'original_patch_probes.bin',out/'surface_probes.bin')
