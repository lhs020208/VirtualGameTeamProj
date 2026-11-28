"""Read-only source/model scaling study; test inputs and patched build stay in Intermediate."""
import csv,hashlib,json,pathlib,shutil,subprocess,sys
root=pathlib.Path(__file__).resolve().parents[1]
out=root/'Docs/DemoScale200k';out.mkdir(parents=True,exist_ok=True)
tmp=root/'Intermediate/DemoScale200k';tmp.mkdir(parents=True,exist_ok=True)
source=root/'SourceAssets/ProcessPlant_20261111_LowPoly/Simulation/test_charge.csv'
rows=list(csv.DictReader(source.open()));fields=list(rows[0])
tiles=[(x,y) for y in (-.112,0,.112) for x in (-.168,-.056,.056,.168) if (x,y) not in ((-.168,-.112),(.168,.112))]
target=tmp/'charge_200k.csv'
with target.open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=fields);w.writeheader();idx=0
 for s in ('0','1'):
  for dx,dy in tiles:
   for r in rows:
    if r['species']!=s:continue
    q=r.copy();q.update(id=idx,x_m=float(r['x_m'])+dx,y_m=float(r['y_m'])+dy);w.writerow(q);idx+=1
assert idx==200000
protected=[*root.glob('Source/VirtualGameTeamProj/PowderPlant*'),root/'Physics/PowderCore/include/powder/demo_plant.hpp',root/'Binaries/Win64/UnrealEditor.modules',root/'Binaries/Win64/Demo20261119_VirtualGameTeamProj_5ed1827b08.dll',root/'Content/ProcessPlant_Demo20261119/L_ProcessPlant_Demo30FPS_1119.umap',source]
receipt=out/'protected_hashes.json'
current={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in protected if p.is_file()}
if receipt.exists():assert json.loads(receipt.read_text())==current
else:receipt.write_text(json.dumps(current,indent=2))
(out/'inputs.json').write_text(json.dumps(dict(original=str(source),scaled=str(target),scaled_sha256=hashlib.sha256(target.read_bytes()).hexdigest(),count=idx,radius_m=.002,density=1200,nominal_mass_kg=200000*4/3*3.141592653589793*.002**3*1200,method='Ten non-overlapping x/y translated tiles within each original source hopper; species ratio and radius unchanged',tiles=tiles),indent=2))
print(target)
