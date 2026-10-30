"""Generate a deliberately approximate OBB passage test, in metres.
Union interiors have no closed ends at overlapping connections. Conical hoppers
use four inclined solid OBB walls. Original rotating rotor and manual gate
contacts are retained. This is NOT a calibrated replacement for source geometry.
"""
from pathlib import Path
import json, struct, math, shutil
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
SRC=ROOT/'SourceAssets/ProcessPlantPhysics'; OUT=ROOT/'SourceAssets/ProcessPlantOBB'
OUT.mkdir(exist_ok=True)
m=json.loads((SRC/'runtime_manifest.json').read_text(encoding='utf-8'))
raw=(SRC/'runtime.plant').read_bytes();d=np.frombuffer(raw,dtype='<f4',offset=4).reshape(-1,4).tolist()
inside=[];solid=[]
def box(group,name,centre,axes,half):
    axes=np.asarray(axes,float);half=np.asarray(half,float)
    assert np.allclose(axes@axes.T,np.eye(3),atol=1e-7) and min(half)>.0013
    group.append(dict(name=name,centre=list(map(float,centre)),axes=axes.tolist(),half=half.tolist()))
def enclosure(name,lo,hi):
    lo=np.array(lo);hi=np.array(hi);box(inside,name,(lo+hi)/2,np.eye(3),(hi-lo)/2)
def tube(name,a,b,ra,rb):
    a=np.array(a,float);b=np.array(b,float);axis=b-a;length=np.linalg.norm(axis);axis/=length
    u=np.cross(axis,[0,0,1] if abs(axis[2])<.9 else [0,1,0]);u/=np.linalg.norm(u);v=np.cross(axis,u)
    # Inscribed square: manual circular valve covers all four corners.
    h=min(ra,rb)/math.sqrt(2);extension=max(.006,h*.35)
    box(inside,name,(a+b)/2,[u,v,axis],[h,h,length/2+extension])
def funnel(name,x,y,z0,z1,r0,r1):
    a0=r0/math.sqrt(2);a1=r1/math.sqrt(2);slope=(a1-a0)/(z1-z0)
    for j in range(4):
        a=j*math.pi/2;radial=np.array([math.cos(a),math.sin(a),0.]);side=np.array([-math.sin(a),math.cos(a),0.]);tangent=radial*slope+np.array([0,0,1.]);tangent/=np.linalg.norm(tangent)
        normal=np.cross(side,tangent);thick=.025
        centre=np.array([x,y,(z0+z1)/2])+radial*(a0+a1)/2+normal*thick
        box(solid,name+str(j),centre,[side,tangent,normal],[a1+.04,math.hypot(z1-z0,a1-a0)/2+.002,thick])
for i,r in enumerate(m['routes'][:3]):
    x,y=r['points'][0][:2];h=.35/math.sqrt(2)
    enclosure('supplier'+str(i),[x-h,y-h,2.298],[x+h,y+h,3.4]);funnel('supplier_funnel'+str(i),x,y,2.3,2.6,.022,.35);tube('supplier_neck'+str(i),[x,y,2.195],[x,y,2.305],.022,.022)
# Explicit test shapes, not final equipment dimensions.
h=.35/math.sqrt(2);enclosure('weighing',[-h,-h,.698],[h,h,1.835]);funnel('weighing_funnel',0,0,.7,.95,.022,.35);tube('weighing_neck',[0,0,.535],[0,0,.705],.022,.022)
h=.245/math.sqrt(2);enclosure('receiver',[3-h,-h,2.255],[3+h,h,3.4]);funnel('receiver_funnel',3,0,2.257,2.502,.139,.245)
enclosure('mixer',[2.25,-.45,.575],[3.75,.45,1.31])
tube('receiver_to_mixer',[3,0,2.17],[3,0,1.30],.139,.139)
box(solid,'filter_proxy',[3,0,3.22],np.eye(3),[.06,.06,.175])
# Simplify nearly straight runs, preserving radius changes and endpoints.
def simplify(points,tol=.001):
    p=np.asarray(points,float)
    if len(p)<=2:return p
    a,b=p[0,:3],p[-1,:3];ab=b-a;den=ab@ab
    if den<1e-12:return np.concatenate([simplify(p[:len(p)//2+1]),simplify(p[len(p)//2:])[1:]])
    t=np.clip((p[:,:3]-a)@ab/den,0,1);err=np.linalg.norm(p[:,:3]-a-t[:,None]*ab,axis=1)
    err=np.maximum(err,abs(p[:,3]-(p[0,3]+t*(p[-1,3]-p[0,3]))));k=int(err.argmax())
    if err[k]<=tol:return p[[0,-1]]
    return np.concatenate([simplify(p[:k+1]),simplify(p[k:])[1:]])
for r in m['routes']:
    points=r['points'][3:] # container and funnel are represented above
    endpoint=[0,0,1.77] if r['phase']==0 else [3,0,2.682]
    if np.linalg.norm(np.array(points[-1][:3])-endpoint)>.001: points=points+[endpoint+[points[-1][3]]]
    p=simplify(points)
    for j,(a,b) in enumerate(zip(p,p[1:])):tube(r['id']+str(j),a[:3],b[:3],a[3],b[3])
    if 'box_cell' in r:
        a,b=r['box_cell'];tube(r['id']+'_entry',a[:3],b[:3],min(a[3],b[3]),min(a[3],b[3]))
def append(group):
    start=len(d)
    for b in group:
        d.append(b['centre']+[0]);d.extend([a+[h] for a,h in zip(b['axes'],b['half'])])
    return start,len(group)
a,n=append(inside);b,k=append(solid);d[9]=[a,n,b,k];d[10]=[1,0,0,0]
# Do not upload or traverse the static source-mesh BVH for this level.
d[0][0]=-1
(OUT/'runtime.plant').write_bytes(struct.pack('<I',len(d))+np.asarray(d,dtype='<f4').tobytes())
for name in ['runtime.routes','test_charge.csv'] : shutil.copyfile(SRC/name,OUT/name)
report=dict(schema='plant-obb-test/1',interiors=inside,solids=solid,rotor='original source triangles',particles=3000,particle_radius_m=.00125,notes=['Approximate square passages inscribed in source circular bores','Union of sphere-eroded OBB interiors; overlapping ends remain open','Four inclined OBBs approximate each hopper funnel','Receiver-to-mixer simplified straight test passage','Original quasi-steady pressure network retained; pressure is not recalibrated to square ducts','No claim of equivalent flow, capacity, mixing quality, or speed guarantee'])
(OUT/'geometry.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('OBB',n,'interiors',k,'solid walls; original rotor retained')
