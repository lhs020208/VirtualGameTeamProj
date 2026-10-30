"""Independent 1D radius-discretization sensitivity; not 3D CFD validation."""
import json,math,csv
from pathlib import Path
import numpy as np
from scipy.optimize import brentq
root=Path(__file__).resolve().parents[1]
data=root/'SourceAssets/ProcessPlantPhysics'
d=np.frombuffer((data/'runtime.plant').read_bytes()[4:],dtype='<f4').reshape(-1,4).astype(float)
model=json.loads((data/'runtime_manifest.json').read_text(encoding='utf-8'))
def loss(q,length,radius,minor,area):
    velocity=q/(area or math.pi*radius**2);reynolds=1.2*velocity*2*radius/1.81e-5
    if reynolds<=0:return 0
    w=max(0,min(1,(reynolds-2300)/1700))
    friction=(64/reynolds)*(1-w)+.3164/reynolds**.25*w
    return (friction*length/(2*radius)+minor)*.6*velocity**2
results=[]
for route in model['routes']:
    row={'route':route['id'],'Q_m3_s_by_refinement':{}}
    for n in [1,2,4,8,16]:
        segments=[]
        for i in route['cells']:
            a,b,flow=d[i:i+3];length=float(np.linalg.norm(b[:3]-a[:3]))
            if flow[2]==1:
                segments.append((length,2*a[3]*b[3]/(a[3]+b[3]),flow[1],4*a[3]*b[3]));continue
            for j in range(n):
                radius=min(a[3]+(b[3]-a[3])*j/n,a[3]+(b[3]-a[3])*(j+1)/n)
                segments.append((length/n,radius,flow[1] if j==0 else 0,0))
        q=brentq(lambda q:sum(loss(q,*s) for s in segments)-1000,1e-10,.1)
        row['Q_m3_s_by_refinement'][str(n)]=q
    row['coarse_vs_16_relative']=abs(row['Q_m3_s_by_refinement']['1']/row['Q_m3_s_by_refinement']['16']-1)
    results.append(row)
report={'drop_Pa':1000,'scope':'1D segment-radius discretization only; not 3D CFD convergence','routes':results}
(root/'Docs/ProcessPlantPhysics/pressure_refinement.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))

# Gauge pressures relative to each route's inlet. Ideal chamber boundary model.
with (root/'Docs/ProcessPlantPhysics/pressure_cells.csv').open('w',newline='',encoding='utf-8') as f:
    writer=csv.writer(f);writer.writerow(['route','cell','phase','Q_m3_s','inlet_gauge_Pa','outlet_gauge_Pa','length_m','hydraulic_radius_m'])
    for route,row in zip(model['routes'],results):
        q=row['Q_m3_s_by_refinement']['1'];pressure=0.
        for k,i in enumerate(route['cells']):
            a,b,flow=d[i:i+3];length=float(np.linalg.norm(b[:3]-a[:3]));rect=flow[2]==1
            radius=2*a[3]*b[3]/(a[3]+b[3]) if rect else min(a[3],b[3]);area=4*a[3]*b[3] if rect else 0
            next_pressure=pressure-loss(q,length,radius,flow[1],area)
            writer.writerow([route['id'],k,route['phase'],q,pressure,next_pressure,length,radius]);pressure=next_pressure
        assert abs(pressure+1000)<1e-5
