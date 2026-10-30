"""Assemble source mesh BVHs and an auditable reduced pipe-flow grid (SI).

No particle path prescription. Route coordinates only define gas cells. Source
walls independently constrain the 3D particles. Explicit test sleeves close
unresolved assembly gaps; never modifies the supplied blend.
"""
import json, math, struct, re
from pathlib import Path
import numpy as np
from collections import defaultdict
ROOT=Path(__file__).resolve().parents[1]
DIR=ROOT/'SourceAssets/ProcessPlantPhysics'
manifest=json.loads((DIR/'manifest.json').read_text(encoding='utf-8'))
packed=[[0.,0.,0.,0.] for _ in range(16)]
selected=[];triangles=[];overrides=[]
def mesh(name):return json.loads((DIR/(name+'.json')).read_text(encoding='utf-8'))
def add_source(name):
    d=mesh(name);v=np.array(d['vertices']);t=np.array(d['triangles'])
    if name=='Collision_WH101_Inspection_Lid':
        v+=np.array([.195,.195,1.837]);overrides.append(dict(kind='translation',source=name,delta_m=[.195,.195,1.837]))
    triangles.extend(v[t].tolist());selected.append(name)
for item in manifest['meshes']:
    n=item['id']
    collision=item['dedicated_collision'] and n not in ['MX-301_Collision_Rotor','CollisionPreview_ASSUMED_VT_MovingGate']
    wall=bool(re.match(r'(P101[A-Z]_(OutletNeck|RigidRoute|WeighingFlexibleJoint)|V101[A-Z]_Body|P201_(Expansion_44|LowRoute)|P202_Reducer|PL_1[234]0_|FH122S_110_Smooth_liner$|DP_1[123]0_|RH201_(110_|120_|121_|130_|140_|210_|211_|215_|Filter_)|BV122_PIPE_(Receiver_side|Weighing_side)|BV(122|272)_11[01]_|(PL_C|RH201_C).*_(Seal|Gasket|Ferrule))',n))
    if collision or wall:add_source(n)
def sleeve(a,b,ra,rb,name):
    a=np.array(a);b=np.array(b);axis=b-a;axis/=np.linalg.norm(axis);u=np.cross(axis,[0,0,1] if abs(axis[2])<.9 else [0,1,0]);u/=np.linalg.norm(u);v=np.cross(axis,u)
    rings=[]
    for p,r in [(a,ra),(b,rb)]:rings.append([p+r*(u*math.cos(t*math.tau/64)+v*math.sin(t*math.tau/64)) for t in range(64)])
    generated=[]
    for j in range(64):
        k=(j+1)%64;generated.extend([[rings[0][j],rings[1][j],rings[1][k]],[rings[0][j],rings[1][k],rings[0][k]]])
    triangles.extend(np.array(generated).tolist());overrides.append(dict(kind='open_sleeve',id=name,a=a.tolist(),b=b.tolist(),radius_a_m=ra,radius_b_m=rb))
# Existing source endpoints leave this 100 mm axial gap. Match both bores.
sleeve([.7,0,1.712],[.7,0,1.812],.061,.065,'Test_PL_cuff_connection')
# Closed valve-body split seams and receiver gasket seats are real interfaces.
sleeve([2.167,0,2.682],[2.173,0,2.682],.064,.064,'Test_BV122_seal')
sleeve([3,0,2.157],[3,0,2.163],.139,.139,'Test_BV272_seal')
def bvh(ts):
    ts=np.asarray(ts,dtype=float);area=np.linalg.norm(np.cross(ts[:,1]-ts[:,0],ts[:,2]-ts[:,0]),axis=1);ts=ts[area>1e-12]
    def node(ids):
        at=len(packed);packed.extend([[0.]*4,[0.]*4]);t=ts[ids];lo=t.min(axis=(0,1));hi=t.max(axis=(0,1))
        if len(ids)<=8:
            start=len(packed);packed.extend([list(v)+[0.] for tri in t for v in tri]);packed[at]=list(lo)+[-len(ids)];packed[at+1]=list(hi)+[start]
        else:
            centres=t.mean(axis=1);axis=int(np.argmax(np.ptp(centres,axis=0)));order=np.argsort(centres[:,axis],kind='stable');mid=len(ids)//2
            left=node(ids[order[:mid]]);right=node(ids[order[mid:]]);packed[at]=list(lo)+[left];packed[at+1]=list(hi)+[right]
        return at
    return node(np.arange(len(ts))),len(ts)
staticroot,staticcount=bvh(triangles)
rotor=mesh('MX-301_Collision_Rotor');rotorroot,rotorcount=bvh(np.array(rotor['vertices'])[np.array(rotor['triangles'])])
def rings(name,n,reverse=False):
    v=np.array(mesh(name)['vertices']);assert len(v)%n==0
    result=[]
    for k in range(0,len(v),n):
        q=v[k:k+n];c=q.mean(axis=0);rad=np.linalg.norm(q-c,axis=1);assert rad.max()-rad.min()<.006
        result.append([*c,float(rad.min())])
    return result[::-1] if reverse else result
def join(*parts):
    out=[]
    for part in parts:
        for p in part:
            if out and math.dist(out[-1][:3],p[:3])<1e-6:out[-1]=p
            else:out.append(p)
    return out
routes=[]
for supplier in manifest['suppliers']:
    name=supplier['id'];letter=name[-1];bounds=supplier['cylinder_bounds_m'];x=(bounds[0][0]+bounds[1][0])/2;y=(bounds[0][1]+bounds[1][1])/2
    pipe=rings('P101'+letter+'_RigidRoute_ID44',96);end=pipe[-1]
    route=join([[x,y,3.3,.35],[x,y,2.6,.35],[x,y,2.3,.022],[x,y,2.2,.022]],pipe,[[*end[:2],1.77,.022]])
    entry=dict(id=name+'->WH101',phase=0,points=route)
    if letter in 'AC':
        radial=np.array(end[:2]);radial/=np.linalg.norm(radial)
        entry['box_cell']=[[*list(radial*.426),1.7115,.035],[*list(radial*.284),1.7115,.0585]]
    routes.append(entry)
transfer=join([[0,0,1.8,.35],[0,0,.95,.35],[0,0,.7,.022],[0,0,.54,.022]],rings('P201_LowRoute_ID44',96),rings('P201_Expansion_44_to_122',96),[[.7,0,1.812,.065]],rings('FH122S_110_Smooth_liner',128,True),[[.7,0,2.282,.061]],rings('PL_130_Long_radius_elbow_R400',128,True),[[1.9,0,2.682,.061],[2.14,0,2.682,.061],[2.2,0,2.682,.061],[2.445,0,2.682,.061],[2.77,0,2.682,.061]])
routes.append(dict(id='WH101->RH201',phase=1,points=transfer))
# Gate data is independent of the number of suppliers.
gates=[]
for supplier in manifest['suppliers']:
    b=supplier['cylinder_bounds_m'];gates.extend([[(b[0][0]+b[1][0])/2,(b[0][1]+b[1][1])/2,2.23,.022],[0,0,0,0]])
gates.extend([[0,0,.6,.022],[1,0,0,0],[3,0,2.16,.139],[2,0,0,0]])
gateoffset=len(packed);packed.extend(gates)
airoffset=len(packed);cells=[]
for route in routes:
    route['cells']=[];points=route['points']
    for a,b in zip(points,points[1:]):
        length=math.dist(a[:3],b[:3]);assert length>1e-7
        # Subdivide long straight cells for pressure sampling; short source bend
        # segments are kept. Minor loss 1 at route inlet is an explicit test value.
        count=max(1,math.ceil(length/.1))
        for k in range(count):
            aa=np.array(a)+(np.array(b)-a)*k/count;bb=np.array(a)+(np.array(b)-a)*(k+1)/count
            i=len(packed);route['cells'].append(i);packed.extend([list(aa),list(bb),[0,1. if len(route['cells'])==1 else 0.,0,route['phase']]])
            cells.append(dict(route=route['id'],a=list(aa),b=list(bb),packed_index=i))
    if 'box_cell' in route:
        aa,bb=route['box_cell'];i=len(packed);route['cells'].append(i);packed.extend([aa,bb,[0,0,1,route['phase']]])
        cells.append(dict(route=route['id'],a=aa,b=bb,packed_index=i,rectangular=True))
packed[0]=[staticroot,rotorroot,airoffset,len(cells)]
packed[1]=[3,0,.9,1200];packed[2]=[-2,-1,-.2,0];packed[3]=[5,2.2,4.5,0];packed[4]=[0,0,0,0];packed[5]=[gateoffset,len(gates)//2,0,0]
# Broad phase for gas cells; exact circular-tube membership is still tested on GPU.
origin=np.array(packed[2][:3]);h=.1;dims=np.ceil((np.array(packed[3][:3])-origin)/h).astype(int);bins=defaultdict(list)
for cell in cells:
    a=np.array(cell['a']);b=np.array(cell['b']);r=max(a[3],b[3]);lo=np.maximum(0,np.floor((np.minimum(a[:3],b[:3])-r-origin)/h).astype(int));hi=np.minimum(dims-1,np.floor((np.maximum(a[:3],b[:3])+r-origin)/h).astype(int))
    for z in range(lo[2],hi[2]+1):
        for y in range(lo[1],hi[1]+1):
            for x in range(lo[0],hi[0]+1):bins[(z*dims[1]+y)*dims[0]+x].append(cell['packed_index'])
gridstart=len(packed);packed.extend([[0.,0.,0.,0.] for _ in range(int(np.prod(dims)))])
for key,ids in bins.items():packed[gridstart+key]=[len(packed),len(ids),0,0];packed.extend([[i,0,0,0] for i in ids])
packed[6]=[gridstart,*map(int,dims)];packed[7]=[*origin,h]
with (DIR/'runtime.plant').open('wb') as f:
    f.write(struct.pack('<I',len(packed)));f.write(np.array(packed,dtype='<f4').tobytes())
(DIR/'runtime.routes').write_text(str(len(routes))+'\n'+'\n'.join(str(r['phase'])+' '+str(len(r['cells']))+' '+' '.join(map(str,r['cells'])) for r in routes)+'\n')
report=dict(schema='plant-runtime/1',source_sha256=manifest['sha256'],model='1D quasi-steady single-phase pipe pressure + 3D approximate granular contacts; one-way gas drag',calibrated=False,static_triangles=staticcount,rotor_triangles=rotorcount,selected_meshes=selected,test_overrides=overrides,flow_cells=cells,routes=routes,particle_file='test_charge.csv',gate_count=len(gates)//2,gate_data=gates,closed_gate_half_thickness_m=.003,scope_limitations=['No resolved 3D turbulence','No powder back reaction/plug pressure loss','No compressible chamber transient','Closed pressure interfaces are boundary assumptions; no full 3D watertight-volume certification'],ready_for_simulation=False)
(DIR/'runtime_manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('PLANT_RUNTIME',staticcount,rotorcount,len(cells),'triangles/rotor/cells',len(packed)*16,'bytes')
