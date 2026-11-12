R"CLC(
#if GRANULAR_PLANT
// Full world coordinates, collision buckets with exact cell-key filtering.
int plant_hash(int3 c){uint h=(uint)c.x*73856093u^(uint)c.y*19349663u^(uint)c.z*83492791u;return h&262143u;}
float3 plant_rotate(float3 p,float a){float s=sin(a),c=cos(a);return (float3)(p.x,c*p.y-s*p.z,s*p.y+c*p.z);}
float3 plant_closest(float3 p,float3 a,float3 b,float3 c){
 float3 ab=b-a,ac=c-a,ap=p-a;float d1=dot(ab,ap),d2=dot(ac,ap);if(d1<=0&&d2<=0)return a;
 float3 bp=p-b;float d3=dot(ab,bp),d4=dot(ac,bp);if(d3>=0&&d4<=d3)return b;
 float vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return a+ab*(d1/(d1-d3));
 float3 cp=p-c;float d5=dot(ab,cp),d6=dot(ac,cp);if(d6>=0&&d5<=d6)return c;
 float vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return a+ac*(d2/(d2-d6));
 float va=d3*d6-d5*d4;if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0)return b+(c-b)*((d4-d3)/((d4-d3)+(d5-d6)));
 float inv=1.f/(va+vb+vc);return a+ab*(vb*inv)+ac*(vc*inv);
}
float3 plant_mesh(float3 p,float r,int root,__global const float4* data,float3* correction){
 if(root<0)return p;
#if PLANT_THREADED
 int at=root==(int)data[0].x?(int)data[8].x:(int)data[8].z;
 int end=root==(int)data[0].x?(int)data[8].y:(int)data[8].w;
 while(at<end){float4 lo=data[at],hi=data[at+1];int next=lo.w>=0?(int)hi.w:at+2;
  if(any(p<lo.xyz-r)||any(p>hi.xyz+r)){at=next;continue;}
  if(lo.w>=0){at=(int)lo.w;continue;}
#else
 int stack[64],top=0;stack[top++]=root;
 while(top){int at=stack[--top];float4 lo=data[at],hi=data[at+1];if(any(p<lo.xyz-r)||any(p>hi.xyz+r))continue;
  if(lo.w>=0){if(top>61)return (float3)(NAN);stack[top++]=(int)lo.w;stack[top++]=(int)hi.w;continue;}
#endif
  for(int j=0;j<(int)(-lo.w);++j){int t=(int)hi.w+3*j;float3 a=data[t].xyz,b=data[t+1].xyz,c=data[t+2].xyz;
#if PLANT_TRIANGLE_FAST
   // Conservative triangle AABB only; original length/contact arithmetic; keep visit/projection order.
   if(any(p<fmin(a,fmin(b,c))-r)||any(p>fmax(a,fmax(b,c))+r))continue;
#endif
   float3 q=plant_closest(p,a,b,c),d=p-q;float l=length(d);if(l<r){
float3 n=l>1.e-9f?d/l:normalize(cross(b-a,c-a));float3 move=n*(r-l);*correction+=move;p+=move;}
  }
#if PLANT_THREADED
 at=next;
#endif
 }return p;
}
// OBB test geometry: union of hollow corridor interiors, minus solid baffles.
// Four records per box: centre, then three unit axes with half extents in w.
float3 plant_box_local(float3 p,int k,__global const float4* d){
 float3 q=p-d[k].xyz;return (float3)(dot(q,d[k+1].xyz),dot(q,d[k+2].xyz),dot(q,d[k+3].xyz));
}
float3 plant_box_world(float3 q,int k,__global const float4* d){return d[k].xyz+q.x*d[k+1].xyz+q.y*d[k+2].xyz+q.z*d[k+3].xyz;}
float3 plant_obb_walls(float3 p,float r,__global const float4* d){
 int start=(int)d[9].x,count=(int)d[9].y;float best=INFINITY;float3 closest=p;
 // Eroded interiors overlap by >2r, so joints have no artificial end wall.
 for(int i=0;i<count;++i){int k=start+4*i;float3 h=(float3)(d[k+1].w,d[k+2].w,d[k+3].w)-r;
  float3 q=plant_box_local(p,k,d),c=clamp(q,-h,h),delta=q-c;float distance=dot(delta,delta);
  if(distance<best){best=distance;closest=plant_box_world(c,k,d);}if(distance<1.e-16f){closest=p;break;}
 }
 p=closest;start=(int)d[9].z;count=(int)d[9].w;
 for(int i=0;i<count;++i){int k=start+4*i;float3 h=(float3)(d[k+1].w,d[k+2].w,d[k+3].w),q=plant_box_local(p,k,d);
  float3 gap=fabs(q)-h,outside=fmax(gap,(float3)(0));float l=length(outside),distance=l+fmin(fmax(gap.x,fmax(gap.y,gap.z)),0.f);
  if(distance<r){float3 n;
   if(l>1.e-9f)n=copysign(outside,q)/l;
   else if(gap.x>=gap.y&&gap.x>=gap.z)n=(float3)(copysign(1.f,q.x),0,0);
   else if(gap.y>=gap.z)n=(float3)(0,copysign(1.f,q.y),0);
   else n=(float3)(0,0,copysign(1.f,q.z));
   p=plant_box_world(q+n*(r-distance),k,d);
  }
 }return p;
}
// Hybrid wall BVH. Leaf references point at exact triangles or solid OBB slabs.
float3 plant_hybrid_shape(float3 p,float r,int k,int type,__global const float4* d,float3* correction){
 if(type==1){float3 a=d[k].xyz,b=d[k+1].xyz,c=d[k+2].xyz;
  if(any(p<fmin(a,fmin(b,c))-r)||any(p>fmax(a,fmax(b,c))+r))return p;
  float3 q=plant_closest(p,a,b,c),delta=p-q;float len=length(delta);
  if(len<r){float3 move=(len>1.e-9f?delta/len:normalize(cross(b-a,c-a)))*(r-len);*correction+=move;p+=move;}return p;
 }
 // Experimental finite annular cylinder: open bore, annular end faces.
 if(type==4){float3 axis=d[k+1].xyz,q=p-d[k].xyz;float z=dot(q,axis);float3 radial=q-z*axis;float rho=length(radial);
  float inner=d[k+2].x-rho,outer=rho-d[k+2].y,dr=fmax(inner,outer),dz=fabs(z)-d[k+1].w;
  float2 out=fmax((float2)(dr,dz),(float2)(0));float len=length(out),distance=len+fmin(fmax(dr,dz),0.f);
  if(distance<r){float3 nr=rho>1.e-9f?radial/rho:(float3)(1,0,0);if(inner>outer)nr=-nr;float3 nz=axis*copysign(1.f,z),n;
   if(dr>0&&dz>0)n=(nr*out.x+nz*out.y)/len;else n=dr>dz?nr:nz;float3 move=n*(r-distance);*correction+=move;p+=move;
  }return p;
 }
 float3 h=(float3)(d[k+1].w,d[k+2].w,d[k+3].w),q=plant_box_local(p,k,d),gap=fabs(q)-h;
 if(any(gap>r))return p;
 if(type==3){
  int edges=(int)d[k+4].x,count=(int)d[k+4].y,inside=0,bestId=2147483647;float best=INFINITY;float2 nearest=q.xy;
  int node=(int)d[k+4].z,end=(int)d[k+4].w;
  // Farther edges cannot contact an outside particle. For an inside particle,
  // an edge beyond max(radius,halfThickness) cannot beat the planar face.
  // Parity still considers every ray-crossing edge, even beyond this radius.
  if(end){float reach=fmax(r,h.z)+1.e-6f;best=reach*reach;}
  // A node can be skipped only if it cannot improve nearest distance AND
  // cannot intersect the positive-X parity ray. Tie-break by source edge ID.
  int linear=0;
  while(node<end||(!end&&linear<count)){
   int first=linear,n=1,next=0;
   if(end){float4 lo=d[node],hi=d[node+1];next=lo.w>=0?(int)hi.w:node+2;
    float2 gap=fmax(fmax(lo.xy-q.xy,q.xy-hi.xy),(float2)(0));
    int ray=q.y>=lo.y&&q.y<=hi.y&&q.x<=hi.x;
    if(dot(gap,gap)>best+1.e-12f&&!ray){node=next;continue;}
    if(lo.w>=0){node=(int)lo.w;continue;}first=(int)hi.w;n=(int)(-lo.w);
   }
   for(int entry=0;entry<n;++entry){int j=end?(int)d[first+entry].x:first;float4 edge=d[edges+j];float2 a=edge.xy,b=edge.zw,ab=b-a;float t=clamp(dot(q.xy-a,ab)/dot(ab,ab),0.f,1.f);float2 point=a+t*ab;float ds=dot(q.xy-point,q.xy-point);if(ds<best||(ds==best&&j<bestId)){best=ds;nearest=point;bestId=j;}
   if((a.y>q.y)!=(b.y>q.y))if(q.x<a.x+(q.y-a.y)*(b.x-a.x)/(b.y-a.y))inside=!inside;
   }
   if(end)node=next;else ++linear;
  }
  float dl=sqrt(best),lateral=inside?-dl:dl,dz=fabs(q.z)-h.z;float2 outside=fmax((float2)(lateral,dz),(float2)(0));float len=length(outside),distance=len+fmin(fmax(lateral,dz),0.f);
  if(distance<r){float2 radial=dl>1.e-9f?(inside?nearest-q.xy:q.xy-nearest)/dl:(float2)(1,0);float3 n;
   if(lateral>0&&dz>0)n=(float3)(radial*outside.x,copysign(outside.y,q.z))/len;
   else if(lateral>dz)n=(float3)(radial,0);else n=(float3)(0,0,copysign(1.f,q.z));
   float3 delta=n*(r-distance);float3 move=delta.x*d[k+1].xyz+delta.y*d[k+2].xyz+delta.z*d[k+3].xyz;*correction+=move;p+=move;
  }return p;
 }

 float3 outside=fmax(gap,(float3)(0));float len=length(outside),distance=len+fmin(fmax(gap.x,fmax(gap.y,gap.z)),0.f);
 if(distance<r){float3 n;
  if(len>1.e-9f)n=copysign(outside,q)/len;
  else if(gap.x>=gap.y&&gap.x>=gap.z)n=(float3)(copysign(1.f,q.x),0,0);
  else if(gap.y>=gap.z)n=(float3)(0,copysign(1.f,q.y),0);
  else n=(float3)(0,0,copysign(1.f,q.z));
  // Add displacement in world coordinates; avoid a lossy world/local round trip.
  float3 delta=n*(r-distance);float3 move=delta.x*d[k+1].xyz+delta.y*d[k+2].xyz+delta.z*d[k+3].xyz;*correction+=move;p+=move;
 }return p;
}
)CLC"
R"CLC(
float3 plant_hybrid_ref(float3 p,float r,float4 ref,__global const float4* d,float3* correction){
#if PLANT_WALL_MODE >= 4
 int k=(int)ref.z;
 if(k>0&&(any(p<d[k].xyz-r)||any(p>d[k+1].xyz+r)))return p;
#endif
 return plant_hybrid_shape(p,r,(int)ref.x,(int)ref.y,d,correction);
}
float3 plant_hybrid_linear(float3 p,float r,int at,int end,__global const float4* d,float3* correction){
 while(at<end){float4 lo=d[at],hi=d[at+1];int next=lo.w>=0?(int)hi.w:at+2;
  if(any(p<lo.xyz-r)||any(p>hi.xyz+r)){at=next;continue;}
  if(lo.w>=0){at=(int)lo.w;continue;}
  for(int j=0;j<(int)(-lo.w);++j){float4 ref=d[(int)hi.w+j];p=plant_hybrid_ref(p,r,ref,d,correction);}
  at=next;
 }return p;
}
// Find the next overlapping ORIGINAL leaf, not the next spatially visited leaf.
// No earlier leaf can change p if its AABB misses. Re-query after each leaf's
// corrections, so newly reachable later leaves are never omitted.
int2 plant_next_leaf(float3 p,float r,int header,int cursor,__global const float4* d){
 int at=(int)d[header].x,end=(int)d[header].y,best=2147483647,leaf=-1;
 while(at<end){float4 lo=d[at],hi=d[at+1],rank=d[at+2];int next=lo.w>=0?(int)hi.w:at+3;
  if(rank.y<cursor||rank.x>=best||any(p<lo.xyz-r)||any(p>hi.xyz+r)){at=next;continue;}
  if(lo.w>=0){at=(int)lo.w;continue;}
  for(int j=0;j<(int)(-lo.w);++j){float4 ref=d[(int)hi.w+j];int id=(int)ref.y,k=(int)ref.x;
   if(id>=cursor&&id<best&&!any(p<d[k].xyz-r)&&!any(p>d[k+1].xyz+r)){best=id;leaf=k;}
  }at=next;
 }return (int2)(leaf,best);
}
#define PLANT_CACHE_CAPACITY 64
#define PLANT_CACHE_TREE_STRIDE 66
#define PLANT_CACHE_STRIDE 139
int plant_cache_base(__global const float4* d){
 return d[10].z>0&&get_global_id(0)<(uint)d[10].w?(int)d[10].z+(int)get_global_id(0)*PLANT_CACHE_STRIDE:-1;
}
void plant_cache_count(__global const float4* d,int counter){
 int base=plant_cache_base(d);if(base<0)return;
 __global uint* counts=(__global uint*)(d+base+138);if(counts[counter]!=0xffffffffu)++counts[counter];
}
float3 plant_hybrid_cached(float3 p,float r,int root,int end,int header,__global const float4* d,float3* correction){
 int base=plant_cache_base(d)+(root==(int)d[12].x?0:PLANT_CACHE_TREE_STRIDE);
 __global float4* cache=(__global float4*)(d+base);float4 anchor=cache[0],meta=cache[1];
 int valid=meta.w==1.f&&meta.y==header&&all(fabs(p-anchor.xyz)+r<=anchor.w-1.e-6f);
 if(!valid){
  plant_cache_count(d,0);float reach=r*3.002f+2.e-6f;int count=0,at=(int)d[header].x,finish=(int)d[header].y;
  cache[0]=(float4)(p,reach);cache[1]=(float4)(-1,header,0,1);
#if PLANT_WALL_MODE >= 6
  at=root==(int)d[12].x?(int)d[9].x:(int)d[9].z;
  finish=root==(int)d[12].x?(int)d[9].y:(int)d[9].w;
  while(at<finish){float4 lo=d[at],hi=d[at+1];int next=lo.w>=0?(int)hi.w:at+2;
   if(any(p<lo.xyz-reach)||any(p>hi.xyz+reach)){at=next;continue;}
   if(lo.w>=0){at=(int)lo.w;continue;}
   for(int j=0;j<(int)(-lo.w);++j){float4 ref=d[(int)hi.w+j];int bound=(int)ref.w;
    if(any(p<d[bound].xyz-reach)||any(p>d[bound+1].xyz+reach))continue;
    int original=(int)ref.x;if(any(p<d[original].xyz-reach)||any(p>d[original+1].xyz+reach))continue;
    int dest=0;while(dest<count&&cache[2+dest].y<ref.y)++dest;
    if(dest<count&&cache[2+dest].y==ref.y){cache[2+dest].z=(float)(((uint)cache[2+dest].z)|((uint)ref.z));continue;}
    if(count==PLANT_CACHE_CAPACITY){plant_cache_count(d,2);return plant_hybrid_linear(p,r,root,end,d,correction);}
    for(int move=count;move>dest;--move)cache[2+move]=cache[2+move-1];cache[2+dest]=ref;++count;
   }at=next;
  }
#else
  while(at<finish){float4 lo=d[at],hi=d[at+1];int next=lo.w>=0?(int)hi.w:at+3;
   if(any(p<lo.xyz-reach)||any(p>hi.xyz+reach)){at=next;continue;}
   if(lo.w>=0){at=(int)lo.w;continue;}
   for(int j=0;j<(int)(-lo.w);++j){float4 ref=d[(int)hi.w+j];int k=(int)ref.x;
    if(any(p<d[k].xyz-reach)||any(p>d[k+1].xyz+reach))continue;
#if PLANT_WALL_MODE >= 5
    uint mask=0;
    for(int shape=0;shape<(int)(-d[k].w);++shape){float4 item=d[(int)d[k+1].w+shape];int bound=(int)item.z;
     if(bound<=0||(!any(p<d[bound].xyz-reach)&&!any(p>d[bound+1].xyz+reach)))mask|=1u<<shape;
    }
    if(mask==0)continue;ref.z=(float)mask;
#endif
    if(count==PLANT_CACHE_CAPACITY){plant_cache_count(d,2);return plant_hybrid_linear(p,r,root,end,d,correction);}
    int dest=count;while(dest>0&&cache[2+dest-1].y>ref.y){cache[2+dest]=cache[2+dest-1];--dest;}cache[2+dest]=ref;++count;
   }at=next;
  }
#endif
  cache[1].x=count;anchor=cache[0];meta=cache[1];
 }else plant_cache_count(d,1);
 // An overflowing list is never used. Keep the exact original traversal.
 if(meta.x<0)return plant_hybrid_linear(p,r,root,end,d,correction);
 for(int i=0;i<(int)meta.x;++i){int at=(int)cache[2+i].x;
  if(any(p<d[at].xyz-r)||any(p>d[at+1].xyz+r))continue;
  for(int j=0;j<(int)(-d[at].w);++j){
#if PLANT_WALL_MODE >= 5
   if(!(((uint)cache[2+i].z)&(1u<<j)))continue;
#endif
   float4 ref=d[(int)d[at+1].w+j];p=plant_hybrid_ref(p,r,ref,d,correction);
#if PLANT_WALL_MODE >= 5
   // The original leaf is already admitted. Finish all remaining shapes if a
   // correction escapes the envelope, then resume at the NEXT original leaf.
   if(any(fabs(p-anchor.xyz)+r>anchor.w-1.e-6f)){
    cache[1].w=0;for(int rest=j+1;rest<(int)(-d[at].w);++rest)p=plant_hybrid_ref(p,r,d[(int)d[at+1].w+rest],d,correction);
    return plant_hybrid_linear(p,r,at+2,end,d,correction);
   }
#endif
  }
  // A correction can leave the cached envelope mid-query. Continue after the
  // fully processed original leaf; never restart/replay earlier contacts.
  if(any(fabs(p-anchor.xyz)+r>anchor.w-1.e-6f)){cache[1].w=0;return plant_hybrid_linear(p,r,at+2,end,d,correction);}
 }return p;
}
float3 plant_hybrid_accelerated(float3 p,float r,int root,int end,__global const float4* d,float3* correction){
#if PLANT_WALL_MODE >= 1
 int header=root==(int)d[12].x?(int)d[13].z:(int)d[13].w;
#if PLANT_WALL_MODE >= 2
 if(header>0&&plant_cache_base(d)>=0)return plant_hybrid_cached(p,r,root,end,header,d,correction);
#endif
 if(header>0){int cursor=0;
  while(cursor<(int)d[header].z){int2 next=plant_next_leaf(p,r,header,cursor,d);if(next.x<0)break;
   int at=next.x;for(int j=0;j<(int)(-d[at].w);++j){float4 ref=d[(int)d[at+1].w+j];p=plant_hybrid_ref(p,r,ref,d,correction);}cursor=next.y+1;
  }return p;
 }
#endif
 return plant_hybrid_linear(p,r,root,end,d,correction);
}
float3 plant_hybrid(float3 p,float r,int root,int end,__global const float4* d,float3* correction){
 float3 result=plant_hybrid_accelerated(p,r,root,end,d,correction);
#if PLANT_BOUNDARY_AUDIT
 // Diagnostic same-input oracle: exact original leaf admission and shape order,
 // without new bounds/masks/cache. Any difference poisons the step, never hides it.
 float3 auditCorrection=(float3)(0);
 int at=root;
 while(at<end){float4 lo=d[at],hi=d[at+1];int next=lo.w>=0?(int)hi.w:at+2;
  if(any(p<lo.xyz-r)||any(p>hi.xyz+r)){at=next;continue;}
  if(lo.w>=0){at=(int)lo.w;continue;}
  for(int j=0;j<(int)(-lo.w);++j){float4 ref=d[(int)hi.w+j];p=plant_hybrid_shape(p,r,(int)ref.x,(int)ref.y,d,&auditCorrection);}at=next;
 }
 if(any(p!=result))return (float3)(NAN);
#endif
 return result;
}
int plant_hybrid_reference_zone(float3 p,float r,__global const float4* d){
 float pad=2*r+.0001f;
 return p.z<d[14].z+pad&&fabs(p.y)<d[14].w+pad&&((p.x>d[14].x-pad&&p.x<d[14].y+pad)||(p.x>d[15].x-pad&&p.x<d[15].y+pad));
}
float3 plant_boundary_ref_uncached(float3 p,float r,float angle,__global const float4* d,float3* wn,float3* wv,float3 reference){
 // Sum geometric projections before rounding them into world positions.
 float3 displacement=(float3)(0);float3* correction=&displacement;
 float3 initial=p;int referenceZone=d[10].x==2.f&&plant_hybrid_reference_zone(p,r,d);p=d[10].x==2.f&&!referenceZone?plant_hybrid(p,r,(int)d[12].x,(int)d[13].x,d,correction):(d[10].x==1.f?plant_obb_walls(p,r,d):plant_mesh(p,r,(int)d[0].x,d,correction));if(d[10].x==2.f&&!referenceZone&&plant_hybrid_reference_zone(p,r,d)){referenceZone=1;displacement=(float3)(0);p=plant_mesh(initial,r,(int)d[0].x,d,correction);}int phase=(int)d[4].x;
 if(d[10].x==1.f)displacement=p-initial;
 // Configurable horizontal valve disks: xyz/radius plus open phase.
 int gates=(int)d[5].x,gatecount=(int)d[5].y;
 for(int j=0;j<gatecount;++j){int k=gates+2*j;int open=phase==(int)d[k+1].x;if(d[k+1].z==1.f)open=d[k+1].y==1.f;
  float4 gate=d[k];if(!open){
   // Finite cylinder: round the rim by the particle radius. A radial entry
   // must not be snapped to a distant top/bottom plane.
   float2 xy=p.xy-gate.xy;float radial=length(xy);float2 axis=radial>1.e-9f?xy/radial:(float2)(1,0);
   float z=p.z-gate.z,dr=radial-gate.w,dz=fabs(z)-.003f;
   float2 outside=fmax((float2)(dr,dz),(float2)(0));float distance=length(outside)+fmin(fmax(dr,dz),0.f);
   if(distance<r){float3 normal;
    if(dr>0.f&&dz>0.f)normal=(float3)(axis.x*outside.x,axis.y*outside.x,copysign(outside.y,z))/length(outside);
    else if(dr>dz)normal=(float3)(axis.x,axis.y,0);
    else normal=(float3)(0,0,copysign(1.f,z));
    float3 move=normal*(r-distance);displacement+=move;p+=move;
   }
  }
 }
 float3 delta=displacement;*wn=length(delta)>1.e-9f?normalize(delta):(float3)(0);*wv=(float3)(0);
 float3 pivot=d[1].xyz,rotated=plant_rotate(p-pivot,-angle)+pivot;
 displacement=(float3)(0);
 float3 moved=d[10].x==2.f&&!referenceZone?plant_hybrid(rotated,r,(int)d[12].y,(int)d[13].y,d,correction):plant_mesh(rotated,r,(int)d[0].y,d,correction),shift=plant_rotate(moved-rotated,angle);
 if(length(displacement)>1.e-9f){p+=shift;*wn=plant_rotate(normalize(displacement),angle);*wv=(float3)(0,-(p.z-pivot.z),p.y-pivot.y);}
 return p;
}
float3 plant_boundary_ref(float3 p,float r,float angle,__global const float4* d,float3* wn,float3* wv,float3 reference){
#if PLANT_WALL_MODE >= 3
 int base=plant_cache_base(d);
 if(base>=0){__global float4* memo=(__global float4*)(d+base+132);
  float4 input=(float4)(p,r),key=(float4)(reference,angle);
  if(memo[5].w==1.f&&memo[5].x==d[4].x&&all(memo[0]==input)&&all(memo[1]==key)){
   plant_cache_count(d,3);*wn=memo[3].xyz;*wv=memo[4].xyz;return memo[2].xyz;
  }
  float3 result=plant_boundary_ref_uncached(p,r,angle,d,wn,wv,reference);
  memo[0]=input;memo[1]=key;memo[2]=(float4)(result,0);memo[3]=(float4)(*wn,0);memo[4]=(float4)(*wv,0);memo[5]=(float4)(d[4].x,0,0,1);return result;
 }
#endif
 return plant_boundary_ref_uncached(p,r,angle,d,wn,wv,reference);
}
float3 plant_boundary(float3 p,float r,float angle,__global const float4* d,float3* wn,float3* wv){return plant_boundary_ref(p,r,angle,d,wn,wv,p);}
float3 plant_air(float3 p,__global const float4* d){
 int start=(int)d[0].z,count=(int)d[0].w,phase=(int)d[4].x;float best=INFINITY;float3 velocity=(float3)(0);
 int3 dims=convert_int3(d[6].yzw),key=convert_int3(floor((p-d[7].xyz)/d[7].w));if(any(key<0)||any(key>=dims))return velocity;
 float4 bucket=d[(int)d[6].x+(key.z*dims.y+key.y)*dims.x+key.x];
 for(int i=0;i<(int)bucket.y;++i){int at=(int)d[(int)bucket.x+i].x;float4 a=d[at],b=d[at+1],q=d[at+2];if((d[4].y!=1.f&&(int)q.w!=phase)||q.x==0.f)continue;
  float3 ab=b.xyz-a.xyz;float l2=dot(ab,ab),t=dot(p-a.xyz,ab)/l2;
  // Finite flow cells have planar ends, not hemispherical caps. Otherwise a
  // large upstream hopper cell can override a narrow downstream nozzle cell.
  if(t<0.f||t>1.f)continue;float radius=mix(a.w,b.w,t);
  if(q.z==1.f){float3 axis=normalize(ab),side=(float3)(-axis.y,axis.x,0),delta=p-(a.xyz+ab*t);float lateral=dot(delta,side);float metric=fmax(fabs(lateral)/a.w,fabs(delta.z)/b.w);
   if(dot(p-a.xyz,axis)>=0&&dot(p-b.xyz,axis)<=0&&metric<1&&metric<best){best=metric;velocity=axis*q.x/(4*a.w*b.w);}continue;}
  float3 radial=p-(a.xyz+ab*t);float dist=length(radial);if(dist<radius&&dist/radius<best){best=dist/radius;
   // Continuity in a tapered streamtube: ur = uz * (r'/r) * radial.
   // The velocity is tangent to its conical wall, not through it.
   velocity=(normalize(ab)+radial*((b.w-a.w)/(sqrt(l2)*radius)))*q.x/(3.14159265359f*radius*radius);}
 }return velocity;
}
#endif
)CLC"

