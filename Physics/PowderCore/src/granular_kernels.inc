R"CLC(
#if GRANULAR_BOUNDS_FAST
#define GRID_OFFSET (float3)(.46f,.26f,.61f)
#else
#define GRID_OFFSET (float3)(.6f,.4f,.65f)
#endif
int cellid(float3 p,float cell,int nx,int ny,int nz){
 int3 q=convert_int3(floor((p+GRID_OFFSET)/cell));
 if(any(q<(int3)(0))||q.x>=nx||q.y>=ny||q.z>=nz)return -1;
 return (q.z*ny+q.y)*nx+q.x;
}
__kernel void cleargrid(__global int* head,int cells){int i=get_global_id(0);if(i<cells)head[i]=-1;}
__kernel void grid(__global const float4* pos,__global int* head,__global int* link,int n,float cell,int nx,int ny,int nz,__global int* error){
 int i=get_global_id(0);if(i>=n)return;int c=cellid(pos[i].xyz,cell,nx,ny,nz);if(c<0){atomic_or(error,1);link[i]=-1;return;}link[i]=atomic_xchg(head+c,i);
}
__kernel void predict(__global float4* pos,__global float4* old,__global float4* vel,int n,float dt,float r,__global int* error,__global float4* predicted){
 int i=get_global_id(0);if(i>=n)return;float3 v=vel[i].xyz+(float3)(0,0,-9.81f)*dt;old[i]=pos[i];
 if(!all(isfinite(v))||length(v)*dt>r*.5f){atomic_or(error,2);return;}
 pos[i].xyz+=v*dt;predicted[i]=pos[i];vel[i]=(float4)(v,0);
}
// Solid oriented box projection, including a well-defined nearest face inside.
float3 boxpush(float3 p,float3 h,float r){
#if GRANULAR_BOUNDS_FAST
 if(any(fabs(p)>h+(float3)(r)))return (float3)(0);
#endif
 float3 q=clamp(p,-h,h),d=p-q;float len=length(d);
 if(len>1.e-9f)return len<r?d*((r-len)/len):(float3)(0);
 float3 gap=h-fabs(p);int axis=gap.x<gap.y?(gap.x<gap.z?0:2):(gap.y<gap.z?1:2);float3 out=(float3)(0);out[axis]=(p[axis]>=0?1.f:-1.f)*(gap[axis]+r);return out;
}
float3 rotatex(float3 p,float a){float s=sin(a),c=cos(a);return (float3)(p.x,c*p.y-s*p.z,s*p.y+c*p.z);}
float3 rotatez(float3 p,float a){float s=sin(a),c=cos(a);return (float3)(c*p.x-s*p.y,s*p.x+c*p.y,p.z);}
float3 boundaries(float3 p,float r,float angle,int gate,float3* wallnormal,float3* wallvelocity){
 float3 start=p;*wallnormal=(float3)(0);*wallvelocity=(float3)(0);
 // Main vessel 0.9 x 0.5 x 0.4 m. A bottom 0.12 x 0.12 m outlet and enclosed receiver.
 if(p.z>=0){p.x=clamp(p.x,-.45f+r,.45f-r);p.y=clamp(p.y,-.25f+r,.25f-r);p.z=fmin(p.z,.4f-r);}
 // Finite floor strips and tube walls resolve corners by nearest solid surface.
 // Conditional floor/tube clamping previously injected velocity at the lip.
 for(int side=-1;side<=1;side+=2){
  p+=boxpush(p-(float3)(side*.255f,0,-.01f),(float3)(.195f,.25f,.01f),r);
  p+=boxpush(p-(float3)(0,side*.155f,-.01f),(float3)(.06f,.095f,.01f),r);
  p+=boxpush(p-(float3)(side*.07f,0,-.125f),(float3)(.01f,.08f,.125f),r);
  p+=boxpush(p-(float3)(0,side*.07f,-.125f),(float3)(.06f,.01f,.125f),r);
 }
 if(!gate)p+=boxpush(p-(float3)(0,0,-.01f),(float3)(.06f,.06f,.01f),r);
 if(p.z<-.25f){p.x=clamp(p.x,-.45f+r,.45f-r);p.y=clamp(p.y,-.25f+r,.25f-r);p.z=fmax(p.z,-.6f+r);}
 float3 shift=p-start;if(length(shift)>1.e-9f)*wallnormal=normalize(shift);
 // Horizontal shaft and six paddles. The physical collision and rendered boxes share these dimensions.
 if(p.z>0){float3 d=p-(float3)(0,0,.21f);float rad=length(d.yz);if(rad<.025f+r){float2 nn=rad>1.e-8f?d.yz/rad:(float2)(0,1);float3 corr=(float3)(0,nn.x,nn.y)*(.025f+r-rad);p+=corr;*wallnormal=normalize(corr);*wallvelocity=(float3)(0,-(p.z-.21f),p.y);}
  for(int k=0;k<6;k++){
#if GRANULAR_BOUNDS_FAST
 if(fabs(p.x-(-.375f+k*.15f))>.07f+r)continue;
#endif
 float a=angle+k*1.04719755f;float3 lp=rotatex(p-(float3)(-.375f+k*.15f,0,.21f),-a);lp.y-=.095f;float beta=(k%2?-.4f:.4f);float3 push=rotatez(boxpush(rotatez(lp,-beta),(float3)(.045f,.07f,.012f),r),beta);if(length(push)>0){float3 corr=rotatex(push,a);p+=corr;*wallnormal=normalize(corr);*wallvelocity=(float3)(0,-(p.z-.21f),p.y);}}
 }
 return p;
}
__kernel void solve(__global const float4* pos,__global const float4* old,__global float4* next,__global const int* head,__global const int* link,int n,float r,float cell,int nx,int ny,int nz,float angle,int gate,float mu){
 int i=get_global_id(0);if(i>=n)return;float3 p=pos[i].xyz;int3 q=convert_int3(floor((p+GRID_OFFSET)/cell));float3 correction=(float3)(0);
 for(int z=-1;z<=1;z++)for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
  int3 c=q+(int3)(x,y,z);if(any(c<(int3)(0))||c.x>=nx||c.y>=ny||c.z>=nz)continue;
  int j=head[(c.z*ny+c.y)*nx+c.x];while(j>=0){if(j!=i){float3 delta=p-pos[j].xyz;float dist=length(delta);if(dist<2*r){float3 normal=dist>1.e-9f?delta/dist:(float3)(i>j?1.f:-1.f,0,0);float overlap=2*r-dist;float3 relative=(p-old[i].xyz)-(pos[j].xyz-old[j].xyz);float3 tangent=relative-normal*dot(relative,normal);float l=length(tangent);correction+=normal*(overlap*.25f);if(l>1.e-9f)correction-=tangent*(fmin(.25f,mu*overlap*.25f/l));}}j=link[j];}
 }
 float l=length(correction);if(l>r*.35f)correction*=r*.35f/l;
 float3 wn,wv;p=boundaries(p+correction,r,angle,gate,&wn,&wv);next[i]=(float4)(p,pos[i].w);
}
__kernel void velocity(__global float4* pos,__global const float4* old,__global float4* vel,int n,float dt,float r,float angle,float omega,int gate,float mu,__global int* error,__global const float4* predicted){
 int i=get_global_id(0);if(i>=n)return;float3 p=pos[i].xyz;float3 v=vel[i].xyz+(p-predicted[i].xyz)/dt,wn,wv;
 // Slightly inflated shell only identifies contact for dissipative wall friction.
 boundaries(p,r*1.002f,angle,gate,&wn,&wv);wv*=omega;
 if(length(wn)>0){float3 relative=v-wv;float vn=dot(relative,wn);float3 tangent=relative-vn*wn;float tl=length(tangent);float normalImpulse=fmax(0.f,-dot((p-old[i].xyz)/dt,wn))+9.81f*dt;
  if(tl>1.e-9f)v-=tangent*fmin(1.f,mu*normalImpulse/tl);if(vn<0)v-=vn*wn;}
 if(!all(isfinite(v))||!all(isfinite(p))){atomic_or(error,4);return;}vel[i]=(float4)(v,0);
}
)CLC"

