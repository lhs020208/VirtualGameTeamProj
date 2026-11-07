"""Source-aligned OBB wall patches + unchanged detailed pipes, spatial BVHs.
No hollow-box union, no prescribed paths. Nonrectangular planar patches retain
polygon boundaries and holes; unconvertible patches retain source triangles.
Units: metres, Blender XYZ.
"""
from pathlib import Path
from collections import defaultdict
import json,math,struct,shutil,argparse
import numpy as np
from scipy.spatial import ConvexHull
ROOT=Path(__file__).resolve().parents[1];SRC=ROOT/'SourceAssets/ProcessPlantPhysics'
parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,default=ROOT/'SourceAssets/ProcessPlantHybrid');parser.add_argument('--spatial',action='store_true',help='REJECTED experiment: changes contact order; diagnostic output only');parser.add_argument('--linear-edges',action='store_true',help='Reference polygon scan for regression');parser.add_argument('--analytic-necks',action='store_true',help='Experimental finite annular cylinders for supply outlet necks');parser.add_argument('--source',type=Path,default=SRC);args=parser.parse_args();SRC=args.source;OUT=args.output;OUT.mkdir(exist_ok=True,parents=True)
if args.spatial and OUT.resolve()==(ROOT/'SourceAssets/ProcessPlantHybrid').resolve():parser.error('Rejected spatial experiment must use a separate --output directory')
manifest=json.loads((SRC/'runtime_manifest.json').read_text(encoding='utf-8'))
if args.analytic_necks and OUT.resolve()==(ROOT/'SourceAssets/ProcessPlantHybrid').resolve():parser.error('Analytic experiment must use a separate --output directory')
HALF_THICKNESS=0.0000025

def patch_shapes(name,verts,triangles,allow_cone=False):
 v=np.asarray(verts,float);t=np.asarray(triangles,int);p=v[t];cross=np.cross(p[:,1]-p[:,0],p[:,2]-p[:,0]);area=np.linalg.norm(cross,axis=1)/2
 good=area>1e-12;t=t[good];p=p[good];area=area[good];cross=cross[good];normals=cross/(area[:,None]*2)
 # Weld only for connectivity grouping. Never move the source coordinates.
 _,weld=np.unique(np.round(v,6),axis=0,return_inverse=True);wt=weld[t];edges=defaultdict(list)
 for i,ids in enumerate(wt):
  for a,b in [(ids[0],ids[1]),(ids[1],ids[2]),(ids[2],ids[0])]:edges[tuple(sorted((a,b)))].append(i)
 parent=np.arange(len(t))
 def find(i):
  while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
  return i
 for ids in edges.values():
  if len(ids)!=2:continue
  a,b=ids
  if normals[a]@normals[b]>math.cos(math.radians(.05)) and np.max(abs((p[b]-p[a,0])@normals[a]))<.000001:
   aa,bb=find(a),find(b)
   if aa!=bb:parent[bb]=aa
 groups=defaultdict(list)
 for i in range(len(t)):groups[find(i)].append(i)
 shapes=[];converted=0;converted_area=0.;cone_boxes=0;max_plane_error=0
 for ids in groups.values():
  ps=np.unique(p[ids].reshape(-1,3),axis=0);n=np.sum(normals[ids]*area[ids,None],axis=0);n/=np.linalg.norm(n);origin=ps.mean(0);planeerr=np.max(abs((ps-origin)@n))
  box=None
  if len(ids)>=2 and planeerr<=.000001:
   u=ps[1]-ps[0];u-=n*(u@n);u/=np.linalg.norm(u);w=np.cross(n,u);xy=np.column_stack(((ps-origin)@u,(ps-origin)@w))
   try:
    hull=xy[ConvexHull(xy).vertices];best=None
    for a,b in zip(hull,np.roll(hull,-1,axis=0)):
     axis=b-a;axis/=np.linalg.norm(axis);side=np.array([-axis[1],axis[0]]);axes=np.array([axis,side]);q=xy@axes.T;lo=q.min(0);hi=q.max(0);rectarea=np.prod(hi-lo)
     if best is None or rectarea<best[0]:best=(rectarea,axes,lo,hi)
    rectarea,axes,lo,hi=best;coverage=area[ids].sum()/rectarea
    cone=allow_cone and .05<abs(n[2])<.99 and np.ptp(ps[:,2])>.05 and len(ps)<=6 and coverage>.45
    if .99999<=coverage<=1.00001:
     worldaxes=np.array([axes[0,0]*u+axes[0,1]*w,axes[1,0]*u+axes[1,1]*w,n]);centre=origin+((lo+hi)/2)@worldaxes[:2];half=np.array([*(hi-lo)/2,HALF_THICKNESS+planeerr])
     if min(half)>.0000001:
      box=dict(_triangles=p[ids].copy(),type=2,centre=centre.tolist(),axes=worldaxes.tolist(),half=half.tolist(),source=name,cone_extension=bool(cone and coverage<.99999));cone_boxes+=int(box['cone_extension']);max_plane_error=max(max_plane_error,planeerr)
    if box is None:
     # A bounded planar OBB with an exact polygon footprint (including holes).
     counts=defaultdict(int);endpoints={}
     for ti in ids:
      for ia,ib in [(0,1),(1,2),(2,0)]:
       wa,wb=wt[ti,ia],wt[ti,ib];edge=tuple(sorted((int(wa),int(wb))));counts[edge]+=1;endpoints[edge]=(p[ti,ia],p[ti,ib])
     boundary=[edge for edge,count in counts.items() if count==1];degree=defaultdict(int)
     for aa,bb in boundary:degree[aa]+=1;degree[bb]+=1
     if len(boundary)>=3 and all(c<=2 for c in counts.values()) and all(n==2 for n in degree.values()):
      worldaxes=np.array([axes[0,0]*u+axes[0,1]*w,axes[1,0]*u+axes[1,1]*w,n]);centre=origin+((lo+hi)/2)@worldaxes[:2];half=np.array([*(hi-lo)/2,HALF_THICKNESS+planeerr]);edges2=[]
      for edge in boundary:
       aa,bb=endpoints[edge];edges2.append([*((aa-centre)@worldaxes[:2].T),*((bb-centre)@worldaxes[:2].T)])
      if min(half)>1e-7:box=dict(_triangles=p[ids].copy(),type=3,centre=centre.tolist(),axes=worldaxes.tolist(),half=half.tolist(),edges=edges2,source=name,cone_extension=False);max_plane_error=max(max_plane_error,planeerr)
   except Exception as e:
    if not isinstance(e,(ValueError,ArithmeticError)):raise
  if box:shapes.append(box);converted+=len(ids);converted_area+=float(area[ids].sum())
  else:
   for i in ids:shapes.append(dict(type=1,vertices=p[i].tolist(),source=name))
 return shapes,dict(source=name,source_triangles=len(t),source_area_m2=float(area.sum()),converted_area_m2=converted_area,converted_triangles=converted,obb_count=sum(s['type']!=1 for s in shapes),retained_triangles=sum(s['type']==1 for s in shapes),cone_extension_boxes=cone_boxes,max_plane_fit_error_m=max_plane_error)

static=[];rotor=[];stats=[]
for name in manifest['selected_meshes']+['MX-301_Collision_Rotor']:
 d=json.loads((SRC/(name+'.json')).read_text(encoding='utf-8'));v=np.array(d['vertices']);t=np.array(d['triangles'])
 for change in manifest['test_overrides']:
  if change.get('source')!=name:continue
  if change['kind']=='translation':v+=np.array(change['delta_m'])
  if change['kind']=='receiver_transform':v=v*[-1,-1,1]+[3,0,1.992]
 selected=name.startswith(('H-101','Collision_WH101_Body','Collision_WH101_Cone','Collision_WH101_Separate_Top','MX-301','RH201_110','RH201_120','RH201_130'))
 target=rotor if name=='MX-301_Collision_Rotor' else static
 if args.analytic_necks and name in ('P101A_OutletNeck','P101B_OutletNeck','P101C_OutletNeck'):
  # Strict fit: two axial end planes and two complete concentric rings only.
  centre=(v.min(0)+v.max(0))/2;rad=np.linalg.norm(v[:,:2]-centre[:2],axis=1);radii=np.unique(np.round(rad,6));levels=np.unique(np.round(v[:,2],6))
  assert len(radii)==2 and len(levels)==2,(name,radii,levels)
  assert np.max(np.min(abs(rad[:,None]-radii[None,:]),axis=1))<1e-6
  angles=np.unique(np.round(np.arctan2(v[:,1]-centre[1],v[:,0]-centre[0]),5));gaps=np.diff(np.r_[angles,angles[0]+2*math.pi]);sagitta=float(radii[-1]*(1-math.cos(gaps.max()/2)))
  assert sagitta<.00006,(name,sagitta)
  target.append(dict(type=4,centre=centre.tolist(),axis=[0,0,1],half_length=float(np.ptp(v[:,2])/2),radii=radii.tolist(),source=name,_triangles=v[t].copy(),sagitta_m=sagitta))
 elif selected:
  shapes,stat=patch_shapes(name,v,t,allow_cone=name.startswith(('H-101','Collision_WH101_Cone','RH201_130')));target.extend(shapes);stats.append(stat)
 else:target.extend(dict(type=1,vertices=tri.tolist(),source=name) for tri in v[t])
for sleeve in manifest['test_overrides']:
 if sleeve['kind']!='open_sleeve':continue
 a=np.array(sleeve['a']);b=np.array(sleeve['b']);axis=b-a;axis/=np.linalg.norm(axis);u=np.cross(axis,[0,0,1] if abs(axis[2])<.9 else [0,1,0]);u/=np.linalg.norm(u);v=np.cross(axis,u)
 rings=[[p+r*(u*math.cos(j*math.tau/64)+v*math.sin(j*math.tau/64)) for j in range(64)] for p,r in [(a,sleeve['radius_a_m']),(b,sleeve['radius_b_m'])]]
 for j in range(64):
  k=(j+1)%64
  for tri in [[rings[0][j],rings[1][j],rings[1][k]],[rings[0][j],rings[1][k],rings[0][k]]]:static.append(dict(type=1,vertices=np.asarray(tri).tolist(),source=sleeve['id']))
raw=np.frombuffer((SRC/'runtime.plant').read_bytes(),dtype='<f4',offset=4).reshape(-1,4);packed=raw[:16].tolist()

# Preserve exact source geometry in narrow mixer shaft/end-wall junction bands.
def copy_reference(at):
 at=int(at);new=len(packed);lo=raw[at].tolist();hi=raw[at+1].tolist();packed.extend([lo,hi])
 if lo[3]>=0:lo[3]=copy_reference(lo[3]);hi[3]=copy_reference(hi[3])
 else:
  start=int(hi[3]);hi[3]=len(packed);packed.extend(raw[start:start+int(-lo[3])*3].tolist())
 return new
reference_roots=[copy_reference(raw[0,0]),copy_reference(raw[0,1])]

def edge_tree(edges):
 # Exact 2D nearest-edge and ray-parity acceleration; original edge IDs retained.
 e=np.asarray(edges,dtype=np.float32);lo=np.minimum(e[:,:2],e[:,2:]);hi=np.maximum(e[:,:2],e[:,2:]);centres=(lo+hi)/2;leaves=[]
 def node(ids):
  at=len(packed);packed.extend([[0.]*4,[0.]*4]);a=lo[ids].min(0);b=hi[ids].max(0)
  if len(ids)<=8:
   packed[at]=[*a,0,-len(ids)];packed[at+1]=[*b,0,0];leaves.append((at,ids))
  else:
   axis=int(np.argmax(np.ptp(centres[ids],axis=0)));ids=ids[np.argsort(centres[ids,axis],kind='stable')];mid=len(ids)//2
   child=node(ids[:mid]);node(ids[mid:]);packed[at]=[*a,0,child];packed[at+1]=[*b,0,len(packed)]
  return at
 root=node(np.arange(len(e)));end=len(packed)
 for at,ids in leaves:packed[at+1][3]=len(packed);packed.extend([[int(i),0,0,0] for i in ids])
 return root,end

def build(shapes,original_root):
 # Preserve the old right-first triangle order. One OBB replaces its patch once.
 from collections import deque
 def key(tri):return tuple(sorted(np.asarray(tri,dtype='<f4').reshape(3,3)[i].tobytes() for i in range(3)))
 lookup=defaultdict(deque)
 for i,shape in enumerate(shapes):
  for tri in shape.get('_triangles',[shape.get('vertices')]):lookup[key(tri)].append(i)
 seen=set();ordered=[];stack=[int(original_root)]
 while stack:
  at=stack.pop();lo,hi=raw[at:at+2]
  if lo[3]>=0:stack.extend([int(lo[3]),int(hi[3])]);continue
  for j in range(int(-lo[3])):
   k=int(hi[3])+3*j;which=lookup[key(raw[k:k+3,:3])].popleft()
   if which not in seen:seen.add(which);ordered.append(shapes[which])
 assert len(ordered)==len(shapes),(len(ordered),len(shapes))
 shapes=ordered
 bounds=[];refs=[]
 for s in shapes:
  at=len(packed)
  if s['type']==1:
   p=np.array(s['vertices']);packed.extend([list(v)+[0.] for v in p]);lo,hi=p.min(0),p.max(0)
  elif s['type']==4:
   c=np.array(s['centre']);a=np.array(s['axis']);half=s['half_length'];radius=s['radii'][1];packed.extend([[*c,0],[*a,half],[*s['radii'],0,0]]);e=half*abs(a)+radius*np.sqrt(1-a*a);lo,hi=c-e,c+e
  else:
   c=np.array(s['centre']);axes=np.array(s['axes']);half=np.array(s['half']);packed.append([*c,0]);packed.extend([list(a)+[h] for a,h in zip(axes,half)]);e=half@abs(axes);lo,hi=c-e,c+e
   if s['type']==3:
    metadata=len(packed);packed.append([metadata+1,len(s['edges']),0,0]);packed.extend(s['edges'])
    if len(s['edges'])>=32 and not args.linear_edges:packed[metadata][2:]=edge_tree(s['edges'])
  refs.append([at,s['type'],0,0]);bounds.append([lo-.000002,hi+.000002])
 bounds=np.array(bounds);centres=bounds.mean(1);leafs=[]
 def node(ids):
  at=len(packed);packed.extend([[0.]*4,[0.]*4]);lo=bounds[ids,0].min(0);hi=bounds[ids,1].max(0)
  if len(ids)<=8:
   packed[at]=[*lo,-len(ids)];packed[at+1]=[*hi,0];leafs.append((at,ids))
  else:
   if args.spatial:
    axis=int(np.argmax(np.ptp(centres[ids],axis=0)));ids=ids[np.argsort(centres[ids,axis],kind='stable')]
   mid=len(ids)//2;first=node(ids[:mid]);node(ids[mid:]);packed[at]=[*lo,first];packed[at+1]=[*hi,len(packed)]
  return at
 root=node(np.arange(len(shapes)));end=len(packed)
 for at,ids in leafs:packed[at+1][3]=len(packed);packed.extend([refs[i] for i in ids])
 return root,end
sr,se=build(static,raw[0,0]);rr,re=build(rotor,raw[0,1])
# Copy and relocate all original gas/gate data, retaining the complete pressure network.
begin=int(raw[5,0]);shift=len(packed)-begin;packed.extend(raw[begin:].tolist());packed[0]=[*reference_roots,float(raw[0,2]+shift),float(raw[0,3])];packed[5]=[float(raw[5,0]+shift),float(raw[5,1]),0,0];packed[6]=[float(raw[6,0]+shift),*map(float,raw[6,1:])];packed[8]=[0]*4;packed[9]=[0]*4;packed[10]=[2,0,0,0];packed[11]=[0]*4;packed[12]=[sr,rr,len(static),len(rotor)];packed[13]=[se,re,0,0];packed[14]=[2.23,2.28,1.32,.46];packed[15]=[3.72,3.77,0,0]
for i in range(int(raw[6,0]),int(raw[6,0]+np.prod(raw[6,1:]))):
 count=int(raw[i,1])
 if count:
  at=int(raw[i,0]);packed[i+shift][0]=at+shift
  for j in range(count):packed[at+j+shift][0]+=shift
(OUT/'runtime.plant').write_bytes(struct.pack('<I',len(packed))+np.asarray(packed,dtype='<f4').tobytes())
routes=manifest['routes'];(OUT/'runtime.routes').write_text(str(len(routes))+'\n'+'\n'.join(str(r['phase'])+' '+str(len(r['cells']))+' '+' '.join(str(i+shift) for i in r['cells']) for r in routes)+'\n')
shutil.copyfile(SRC/'test_charge.csv',OUT/'test_charge.csv')
report=dict(schema='plant-hybrid/1',static_shapes=len(static),rotor_shapes=len(rotor),static_obbs=sum(s['type']!=1 for s in static),rotor_obbs=sum(s['type']!=1 for s in rotor),source_static_triangles=manifest['static_triangles'],source_rotor_triangles=manifest['rotor_triangles'],patch_half_thickness_m=HALF_THICKNESS,meshes=stats,spatial_search=('experimental spatial median BVH' if args.spatial else 'source-order stackless AABB BVH')+('; linear edge reference' if args.linear_edges else '; exact 2D edge BVH for complex clipped patches'),obb_semantics='thin source-aligned solid wall slabs; nonrectangular patches clipped to original polygon edges and holes; original mesh fallback at mixer shaft/end interfaces',pipe_geometry='unchanged source triangles and connection sleeves',pressure='unchanged full network',particles=sum(1 for _ in (OUT/'test_charge.csv').open())-1)
report['analytic_cylinders']=[{k:v for k,v in s.items() if k!='_triangles'} for s in static if s['type']==4]
if args.analytic_necks:report['pipe_geometry']='experimental analytic supplier outlet necks; other pipe triangles unchanged'
(OUT/'manifest.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
(OUT/'obb_geometry.json').write_text(json.dumps(dict(static=[{k:v for k,v in s.items() if k!='_triangles'} for s in static if s['type']!=1],rotor=[{k:v for k,v in s.items() if k!='_triangles'} for s in rotor if s['type']!=1]),indent=2),encoding='utf-8')
# Surface probes, including seams, for comparison against the reference GPU boundary.
probes=[]
for shapes in [static,rotor]:
 for s in shapes:
  if s['type']==1:continue
  tris=s['_triangles']
  # Approach source faces from their outward (free-space) side, not from wall bulk.
  for tri in [tris[0],tris[-1]]:
   n=np.cross(tri[1]-tri[0],tri[2]-tri[0]);n/=np.linalg.norm(n);probes.append([*(tri.mean(0)+.001*n),float(shapes is rotor)])
np.asarray(probes,dtype='<f4').tofile(OUT/'surface_probes.bin')
print(json.dumps(report,indent=2))
