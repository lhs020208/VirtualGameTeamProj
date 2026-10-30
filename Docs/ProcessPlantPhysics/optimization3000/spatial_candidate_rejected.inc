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
float3 plant_mesh_full(float3 p,float r,int root,__global const float4* data){
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
float3 n=l>1.e-9f?d/l:normalize(cross(b-a,c-a));p+=n*(r-l);}
  }
#if PLANT_THREADED
 at=next;
#endif
 }return p;
}
// Static geometry candidates are reusable across particles/steps in the same cell.
// Original right-first leaf order is retained. Escaping the cell restarts the
// original traversal from the original point, including projection-induced motion.
float3 plant_mesh(float3 p,float r,int root,__global const float4* d){
 if(root!=(int)d[0].x||d[9].x<=0||r>d[11].x||any(p<d[12].xyz)||any(p>d[13].xyz))return plant_mesh_full(p,r,root,d);
 int3 dims=convert_int3(d[9].yzw),key=convert_int3(floor((p-d[10].xyz)/d[10].w));
 if(any(key<0)||any(key>=dims))return plant_mesh_full(p,r,root,d);
 float3 original=p,lower=d[10].xyz+convert_float3(key)*d[10].w,upper=lower+d[10].w;
 float4 bucket=d[(int)d[9].x+(key.z*dims.y+key.y)*dims.x+key.x];
 for(int i=0;i<(int)bucket.y;++i){int at=(int)d[(int)bucket.x+i].x;float4 lo=d[at],hi=d[at+1];if(any(p<lo.xyz-r)||any(p>hi.xyz+r))continue;
  for(int j=0;j<(int)(-lo.w);++j){int t=(int)hi.w+3*j;float3 a=d[t].xyz,b=d[t+1].xyz,c=d[t+2].xyz;
   float3 q=plant_closest(p,a,b,c),delta=p-q;float l=length(delta);if(l<r){float3 n=l>1.e-9f?delta/l:normalize(cross(b-a,c-a));p+=n*(r-l);
    if(any(p<lower)||any(p>=upper))return plant_mesh_full(original,r,root,d);
   }
  }
 }return p;
}
float3 plant_boundary_ref(float3 p,float r,float angle,__global const float4* d,float3* wn,float3* wv,float3 reference){
 float3 initial=p;p=plant_mesh(p,r,(int)d[0].x,d);int phase=(int)d[4].x;
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
    p+=normal*(r-distance);
   }
  }
 }
 float3 delta=p-initial;*wn=length(delta)>1.e-9f?normalize(delta):(float3)(0);*wv=(float3)(0);
 float3 pivot=d[1].xyz,rotated=plant_rotate(p-pivot,-angle)+pivot;
 float3 moved=plant_mesh(rotated,r,(int)d[0].y,d),shift=plant_rotate(moved-rotated,angle);
 if(length(shift)>1.e-9f){p+=shift;*wn=normalize(shift);*wv=(float3)(0,-(p.z-pivot.z),p.y-pivot.y);}
 return p;
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

