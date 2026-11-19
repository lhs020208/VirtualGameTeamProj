"""Read-only audit of the actual LowPoly collision BVH, gas route and charge.

Run from the project root. Evidence is written only under Saved; runtime inputs
and the physics core are never changed. Distances are solver metres.
"""
import csv
import hashlib
import json
import math
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / 'SourceAssets/ProcessPlant_20261111_LowPoly/Simulation'
raw = (DATA / 'runtime.plant').read_bytes()
count, = struct.unpack_from('<I', raw)
assert len(raw) == 4 + count * 16

def row(i):
    return struct.unpack_from('<4f', raw, 4 + 16 * i)

def sub(a, b): return tuple(x-y for x,y in zip(a,b))
def dot(a, b): return sum(x*y for x,y in zip(a,b))
def cross(a, b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])

def ray(origin, direction):
    """Nearest source triangle along the ray, traversing the runtime's BVH."""
    nearest = 2.0
    stack = [int(row(0)[0])]
    while stack:
        node = stack.pop()
        lo,hi = row(node),row(node+1)
        t0,t1 = 0.0,nearest
        for i in range(3):
            if abs(direction[i]) < 1e-12:
                if not lo[i]-1e-7 <= origin[i] <= hi[i]+1e-7: t1=-1; break
            else:
                a,b=(lo[i]-origin[i])/direction[i],(hi[i]-origin[i])/direction[i]
                t0,t1=max(t0,min(a,b)),min(t1,max(a,b))
        if t1 < t0: continue
        if lo[3]>=0:
            stack.extend([int(lo[3]),int(hi[3])]);continue
        for i in range(int(-lo[3])):
            at=int(hi[3])+3*i
            a,b,c=(row(at+j)[:3] for j in range(3))
            e1,e2=sub(b,a),sub(c,a);h=cross(direction,e2);det=dot(e1,h)
            if abs(det)<1e-13: continue
            s=sub(origin,a);u=dot(s,h)/det
            if u < -1e-6 or u > 1+1e-6: continue
            q=cross(s,e1);v=dot(direction,q)/det
            if v < -1e-6 or u+v > 1+1e-6: continue
            t=dot(e2,q)/det
            if 1e-7<t<nearest:nearest=t
    return nearest

routes=[]
for line in (DATA/'runtime.routes').read_text().splitlines()[1:]:
    values=list(map(int,line.split()));assert values[1]==len(values)-2
    if values[0]==1:routes.append(values[2:])
assert len(routes)==1
cells=[{'index':i,'a':row(i),'b':row(i+1)} for i in routes[0]]
assert all(math.dist(cells[i]['b'][:3],cells[i+1]['a'][:3])<1e-5 for i in range(len(cells)-1))
profile=[(2.16,.136),(2.292,.136),(2.307,.141),(2.487,.246),(3.63,.246),
         (3.641,.242),(3.662,.215),(3.679,.15),(3.687,.07),(3.688,.0001)]
samples=[]
for x,z,expected in [(0,z,.022 if z<.7 else .022+(z-.7)/.25*(.35-.022) if z<.95 else .35) for z in [.65,.75,.85,1.1,1.6]] + [
    (3,(a[0]+b[0])/2,(a[1]+b[1])/2) for a,b in zip(profile,profile[1:])]:
    # Start outside the internal filter when checking the receiver outer shell.
    offset=.12 if x==3 and 3.037<=z<3.4 else 0
    hits=[offset+ray((x+offset*math.cos(angle),offset*math.sin(angle),z),(math.cos(angle),math.sin(angle),0)) for angle in [.4,1.2,2.0,4.8]]
    samples.append({'center':[x,0,z],'profile_radius_m':expected,'source_wall_rays_m':hits,'max_difference_m':max(abs(r-expected) for r in hits)})

charges=list(csv.DictReader((DATA/'test_charge.csv').open()))
radii={float(p['radius_m']) for p in charges};density={float(p['density_kg_m3']) for p in charges}
assert len(radii)==len(density)==1
radius=struct.unpack('<f',struct.pack('<f',radii.pop()))[0]
rho=density.pop();assert rho==row(1)[3]
mass=4*math.pi*radius**3*rho/3
assert [int(p['id']) for p in charges]==list(range(len(charges)))
report={'data_directory':str(DATA.relative_to(ROOT)),'sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in DATA.iterdir() if p.is_file()},
        'sphere_radius_m_as_solver_float':radius,'density_kg_m3':rho,'particle_mass_kg':mass,'particles':len(charges),'total_model_mass_kg':mass*len(charges),
        'representative_multiplier':1,'batch_conversion_supported':False,'transfer_cell_count':len(cells),'transfer_first':cells[0],'transfer_last':cells[-1],
        'wall_profile_checks':samples,'max_profile_difference_m':max(s['max_difference_m'] for s in samples),
        'receiver_filter_boundaries':{'bottom_cap_z':2.9+ray((3.08,0,2.9),(0,0,1)),
            'core_outer_radius':.12-ray((3.12,0,3.1),(-1,0,0)),
            'tube_sheet_z':3.2+ray((3.15,0,3.2),(0,0,1)),
            'powder_region_top_z':3.39,'clean_air_module_and_filter_bore_excluded':True},
        'boundary_method':'Particle center, half-open gate/top planes; weighing then receiver then route union; inlet spigot excluded from receiver until route end plane.'}
out=ROOT/'Saved/LowPolyMassTransferValidation';out.mkdir(parents=True,exist_ok=True)
assert report['max_profile_difference_m']<.0001, 'Receiver/WH nominal profile differs from current collision data'
(out/'geometry_mass_audit.json').write_text(json.dumps(report,indent=2))
print(json.dumps({k:report[k] for k in ['particle_mass_kg','total_model_mass_kg','transfer_cell_count','max_profile_difference_m']}))
for sample in samples:
    print(sample)
