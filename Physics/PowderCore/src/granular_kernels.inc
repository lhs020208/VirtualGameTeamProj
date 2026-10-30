R"CLC(
float grain_radius(float group,float legacy){
#if GRANULAR_BINARY
 return group==0?MAT_R0:MAT_R1;
#else
 return legacy;
#endif
}
float grain_wall_mu(float group,float legacy){
#if GRANULAR_BINARY
 return group==0?MAT_W0:MAT_W1;
#else
 return legacy;
#endif
}
float3 pair_correction(float4 a,float4 b,float3 olda,float3 oldb,int i,int j,float r,float mu
#if GRANULAR_ROTATION
 ,float3 ai,float3 aj,float3* angular
#endif
 ){
 float reach=grain_radius(a.w,r)+grain_radius(b.w,r);float3 delta=a.xyz-b.xyz;float dist2=dot(delta,delta);
 if(dist2>=reach*reach)return (float3)(0);
 float dist=sqrt(dist2);float3 normal=dist>1.e-9f?delta/dist:(float3)(i>j?1.f:-1.f,0,0);float overlap=reach-dist;
 float weight=.25f;
#if GRANULAR_BINARY
 float mi=a.w==0?MAT_M0:MAT_M1,mj=b.w==0?MAT_M0:MAT_M1;
 weight=.5f*mj/(mi+mj);mu=a.w!=b.w?MAT_CROSS:(a.w==0?MAT_MU0:MAT_MU1);
#endif
 float3 relative=(a.xyz-olda)-(b.xyz-oldb);
#if GRANULAR_ROTATION
 float ri=grain_radius(a.w,r),rj=grain_radius(b.w,r);
 relative-=cross(ai*ri+aj*rj,normal);
#endif
 float3 tangent=relative-normal*dot(relative,normal);float l=length(tangent);
#if GRANULAR_ROTATION
 float3 tc=l>1.e-9f?-tangent*fmin(weight/3.5f,mu*overlap*weight/l):(float3)(0);
 *angular-=2.5f/ri*cross(normal,tc);
 // Rolling resistance dissipates relative tangent angular motion, equal/opposite torque.
 float3 wr=ai-aj;wr-=normal*dot(wr,normal);float wl=length(wr);
 float ii=.4f*mi*ri*ri,ij=.4f*mj*rj*rj;
 float rolling_limit=MAT_ROLL*overlap*.5f*(mi*mj/(mi+mj))*(ri*rj/(ri+rj))/ii;
 if(wl>1.e-9f)*angular-=wr*fmin(.5f*ij/(ii+ij),rolling_limit/wl);
 return normal*(overlap*weight)+tc;
#else
 return normal*(overlap*weight)-(l>1.e-9f?tangent*fmin(weight,mu*overlap*weight/l):(float3)(0));
#endif
}

#if GRANULAR_MX301
#define GRID_OFFSET (float3)(.8f,.5f,.5f)
#elif GRANULAR_BOUNDS_FAST
#define GRID_OFFSET (float3)(.46f,.26f,.61f)
#else
#define GRID_OFFSET (float3)(.6f,.4f,.65f)
#endif
int candidate_index(int i,int k,int n,int capacity){
#if GRANULAR_KERNEL_OPTIMIZE
 return k*n+i;
#else
 return i*capacity+k;
#endif
}
int cellid(float3 p,float cell,int nx,int ny,int nz){
 int3 q=convert_int3(floor((p+GRID_OFFSET)/cell));
#if GRANULAR_PLANT
 return plant_hash(q);
#endif
 if(any(q<(int3)(0))||q.x>=nx||q.y>=ny||q.z>=nz)return -1;
 return (q.z*ny+q.y)*nx+q.x;
}
__kernel void cleargrid(__global int* head,int cells
#if GRANULAR_REUSE
 ,__global const unsigned* control
#endif
 ){
#if GRANULAR_REUSE
 if(!control[0])return;
#endif
 int i=get_global_id(0);if(i<cells)head[i]=-1;
}
__kernel void grid(__global const float4* pos,__global int* head,__global int* link,int n,float cell,int nx,int ny,int nz,__global int* error
#if GRANULAR_REUSE
 ,__global const unsigned* control
#endif
 ){
#if GRANULAR_REUSE
 if(!control[0])return;
#endif
 int i=get_global_id(0);if(i>=n)return;int c=cellid(pos[i].xyz,cell,nx,ny,nz);if(c<0){atomic_or(error,1);link[i]=-1;return;}link[i]=atomic_xchg(head+c,i);
}
#if GRANULAR_COUPLED_AIR
__kernel void clear_gas(__global int* head,int n){int i=get_global_id(0);if(i<n)head[i]=-1;}
__kernel void reduce_gas(__global const int* head,__global const int* link,__global const float4* old,__global const float4* impulse,__global float4* exchange,__global const float4* gradient,float4 origin,int4 dims,float h,__global int* error,int particles){
 int i=get_global_id(0);if(i>=dims.x*dims.y*dims.z||error[0])return;if(gradient[i].w==0)return;int3 target=(int3)(i%dims.x,(i/dims.x)%dims.y,i/(dims.x*dims.y));float4 sum=(float4)(0);float rate=0;
 for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x){int3 cell=target-(int3)(x,y,z);if(any(cell<(int3)(0)))continue;int id=(cell.z*dims.y+cell.y)*dims.x+cell.x;
  for(int p=head[id];p>=0;p=link[p]){float3 q=(old[p].xyz-origin.xyz)/h-.5f;int3 base=convert_int3(floor(q));float3 f=q-floor(q),weight=(float3)(0);for(int k=0;k<3;++k){if(clamp(base[k],0,dims[k]-1)==target[k])weight[k]+=1-f[k];if(clamp(base[k]+1,0,dims[k]-1)==target[k])weight[k]+=f[k];}float w=weight.x*weight.y*weight.z;sum+=w*impulse[p];rate+=w*impulse[p+particles].x;}
 }
 exchange[i]+=sum;exchange[i+dims.x*dims.y*dims.z].x+=rate;
}
#endif
__kernel void predict(__global float4* pos,__global float4* old,__global float4* vel,int n,float dt,float r,__global int* error,__global float4* predicted
#if GRANULAR_PLANT
 ,__global const float4* plant
#endif
#if GRANULAR_ROTATION
 ,__global float4* angular,__global const float4* spin
#endif
#if GRANULAR_COUPLED_AIR
 ,__global const float4* gas_velocity,__global const float4* gas_gradient,__global float4* gas_exchange,float4 gas_origin,int4 gas_size,float gas_h,float gas_mu,__global float4* particle_impulse,__global int* gas_head,__global int* gas_link
#endif
#if GRANULAR_COHESION
 ,__global const float4* adhesive_acceleration,__global float4* contact_correction
#endif
 ){
 int i=get_global_id(0);if(i>=n)return;r=grain_radius(pos[i].w,r);float3 v;float3 external=(float3)(0,0,-9.81f);
#if GRANULAR_COHESION
 external+=adhesive_acceleration[i].xyz;contact_correction[i]=(float4)(0);
#endif
#if GRANULAR_COUPLED_AIR
 float3 p=(pos[i].xyz-gas_origin.xyz)/gas_h;
 if(any(p<(float3)(0))||any(p>convert_float3(gas_size.xyz))){atomic_or(error,64);return;}
 int3 cell=min(convert_int3(floor(p)),gas_size.xyz-1);int ci=(cell.z*gas_size.y+cell.y)*gas_size.x+cell.x;
 if(gas_gradient[ci].w==0){atomic_or(error,64);return;}
 float3 q=p-.5f,fract=q-floor(q);int3 base=convert_int3(floor(q));int ids[8];float ws[8];float sum=0;int j=0;
 for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x){int3 a=clamp(base+(int3)(x,y,z),(int3)(0),gas_size.xyz-1);ids[j]=(a.z*gas_size.y+a.y)*gas_size.x+a.x;ws[j]=gas_gradient[ids[j]].w>0?(x?fract.x:1-fract.x)*(y?fract.y:1-fract.y)*(z?fract.z:1-fract.z):0;sum+=ws[j];++j;}
 if(sum<=0){atomic_or(error,64);return;}
 float4 a=(float4)(0),b=(float4)(0);for(int j=0;j<8;++j){ws[j]/=sum;a+=ws[j]*gas_velocity[ids[j]];b+=ws[j]*gas_gradient[ids[j]];}
 float rho=pos[i].w==0?AIR_SOLID_RHO0:AIR_SOLID_RHO1,d=2*r,slip=length(a.xyz-vel[i].xyz),re=a.w*d*slip/gas_mu,eps=b.w,er=eps*re;
 float cdre=er<=1000?24*(1+.15f*pow(er,.687f)):.44f*er;
 float beta=eps<.8f?gas_mu/(rho*eps*d*d)*(150*(1-eps)/eps+1.75f*re):.75f*gas_mu*cdre*pow(eps,-2.65f)/(rho*eps*d*d);
 if(!isfinite(beta)||slip>100||re>200000||beta*dt>.1f||eps<.35f||eps>1.000001f){atomic_or(error,32);return;}
 float3 gravity=external;v=(vel[i].xyz+dt*(gravity+beta*a.xyz-b.xyz/rho))/(1+beta*dt);
 float volume=4.188790204786391f*r*r*r,mass=volume*rho;
 float3 impulse=mass*(v-vel[i].xyz-dt*gravity);
 particle_impulse[i]=(float4)(-impulse,volume)/sum;particle_impulse[i+n]=(float4)(mass*beta/(1+beta*dt)/sum,0,0,0);
 int3 bucket=clamp(base,(int3)(0),gas_size.xyz-1);int bin=(bucket.z*gas_size.y+bucket.y)*gas_size.x+bucket.x;gas_link[i]=atomic_xchg(gas_head+bin,i);
#elif GRANULAR_PLANT
 float3 air=plant_air(pos[i].xyz,plant);float rho=plant[1].w,re=1.2f*(2*r)*length(air-vel[i].xyz)/1.81e-5f;float cdre=re<1000?24*(1+.15f*pow(re,.687f)):.44f*re;float beta=3*1.81e-5f*cdre/(16*rho*r*r);v=(vel[i].xyz+dt*(external+beta*air))/(1+dt*beta);
 if(!isfinite(beta)||length(air-vel[i].xyz)>100.f||re>200000.f||beta*dt>.1f){atomic_or(error,32);return;}
 if(any(pos[i].xyz<plant[2].xyz)||any(pos[i].xyz>plant[3].xyz)){atomic_or(error,1);return;}
#elif GRANULAR_AIR
 float rho=pos[i].w==0?AIR_SOLID_RHO0:AIR_SOLID_RHO1;
 float3 air=(float3)(AIR_U0,AIR_U1,AIR_U2);
 float slip=length(vel[i].xyz-air),re=AIR_RHO*(2*r)*slip/AIR_MU;
 float cdre=re<=1000.f?24.f*(1.f+.15f*pow(re,.687f)):.44f*re;
 float beta=3.f*AIR_MU*cdre/(16.f*rho*r*r);
 if(!isfinite(beta)||!isfinite(re)||slip>100.f||re>200000.f||beta*dt>.25f){atomic_or(error,32);return;}
 v=(vel[i].xyz+dt*(external+(float3)(0,0,9.81f)*AIR_RHO/rho+beta*air))/(1.f+dt*beta);
#else
 v=vel[i].xyz+external*dt;
#endif
 old[i]=pos[i];
 if(!all(isfinite(v))||length(v)*dt>r*.5f){atomic_or(error,2);return;}

#if GRANULAR_ROTATION
 angular[i]=(float4)(spin[i].xyz*dt,0);
#endif
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
// Derived on-device from the exact plane/part constants used by narrow phase.
__kernel void init_boundary_groups(__global float4* bounds){
 int g=get_global_id(0);if(g>=22)return;float3 lo=(float3)(INFINITY),hi=(float3)(-INFINITY);float minw=INFINITY;
 if(g<17){for(int j=g*8;j<min(133,g*8+8);++j){float4 a=mxplanes[j];lo=fmin(lo,a.xyz);hi=fmax(hi,a.xyz);minw=fmin(minw,a.w);}}
 else{int start=(g-17)*4;for(int k=start;k<min(17,start+4);++k){float8 b=mxparts[k];lo=fmin(lo,(float3)(b.s2,b.s3,b.s4));hi=fmax(hi,(float3)(b.s5,b.s6,b.s7));}minw=0;}
 bounds[2*g]=(float4)(lo,minw);bounds[2*g+1]=(float4)(hi,0);
}
float3 boundary_eval(float3 p,float r,float angle,int gate,float3* wallnormal,float3* wallvelocity,__global const float4* bounds,int* work){
 float3 start=p;*wallnormal=(float3)(0);*wallvelocity=(float3)(0);
#if GRANULAR_PLANT
 return plant_boundary(p,r,angle,bounds,wallnormal,wallvelocity);
#elif GRANULAR_SIMPLE_BOX
 float3 original=p;p=clamp(p,(float3)(-.8f+r,-.5f+r,-.5f+r),(float3)(.8f-r,.5f-r,.5f-r));float3 delta=p-original;if(length(delta)>0)*wallnormal=normalize(delta);return p;
#elif GRANULAR_MX301
 // Closed test-only interior and conservative convex rotor components from source asset.
 for(int group=0;group<17;++group){
#if GRANULAR_BOUNDARY_CANDIDATES
  float4 lo=bounds[2*group],hi=bounds[2*group+1];
  // max_j(dot(n_j,p)-w_j+r) <= dot(extremeNormal,p)-min(w_j)+r.
  // With no active plane in this group, p cannot change within the group.
  float3 extreme=select(lo.xyz,hi.xyz,p>=(float3)(0));
  float upper=dot(extreme,p)-lo.w+r;
  if(upper < -1.e-5f*(1.f+length(p)))continue;
#endif
  for(int j=group*8;j<min(133,group*8+8);++j){++work[0];float4 a=mxplanes[j];float d=dot(a.xyz,p)-a.w+r;if(d>0)p-=a.xyz*d;}
 }
 float3 shift=p-start;if(length(shift)>1.e-9f)*wallnormal=normalize(shift);
 float3 localpoint=rotatex(p,-angle),initial=localpoint;
 for(int k=0;k<17;k++){
#if GRANULAR_BOUNDARY_CANDIDATES
  if(k%4==0){int group=17+k/4;float3 lo=bounds[2*group].xyz,hi=bounds[2*group+1].xyz;float margin=1.e-5f*(1.f+length(localpoint));
   if(any(localpoint<lo-r-margin)||any(localpoint>hi+r+margin)){k=min(k+3,16);continue;}
  }
#endif
  ++work[1];float8 b=mxparts[k];if(any(localpoint<(float3)(b.s2,b.s3,b.s4)-r)||any(localpoint>(float3)(b.s5,b.s6,b.s7)+r))continue;
  float distance=-1.e9f;float3 normal=(float3)(0);for(int j=(int)b.s0;j<(int)(b.s0+b.s1);j++){float4 a=mxplanes[j];float d=dot(a.xyz,localpoint)-a.w;if(d>distance){distance=d;normal=a.xyz;}if(distance>=r)break;}
  if(distance<r)localpoint+=normal*(r-distance);
 }
 float3 push=rotatex(localpoint-initial,angle);if(length(push)>1.e-9f){p+=push;*wallnormal=normalize(push);*wallvelocity=(float3)(0,-p.z,p.y);}return p;
#else

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
#endif
}
float3 boundaries(float3 p,float r,float angle,int gate,float3* wn,float3* wv,__global const float4* bounds){int work[2]={0,0};return boundary_eval(p,r,angle,gate,wn,wv,bounds,work);}
#if GRANULAR_COHESION
// Contact-only attraction combined with rigid positional repulsion: NOT full Hertz-DMT.
float3 cohesive_force(float4 a,float4 b,float r){float ri=grain_radius(a.w,r),rj=grain_radius(b.w,r);float3 d=b.xyz-a.xyz;float distance=length(d);return distance>1e-9f&&distance<=ri+rj+1e-5f*fmin(ri,rj)?d/distance*(12.56637061435917f*COHESION_GAMMA*ri*rj/(ri+rj)):(float3)(0);}
__kernel void adhesive_loads(__global const float4* pos,__global const int* head,__global const int* link,int n,float r,float cell,int nx,int ny,int nz,__global float4* acceleration,__global int* error
#if GRANULAR_CANDIDATES
 ,__global const int* candidates,__global const int* counts,int capacity
#endif
 ){
 int i=get_global_id(0);if(i>=n)return;float ri=grain_radius(pos[i].w,r),mi=pos[i].w==0?MAT_M0:MAT_M1;float3 p=pos[i].xyz,force=(float3)(0);int3 q=convert_int3(floor((p+GRID_OFFSET)/cell));
 if(COHESION_GAMMA>0){
#if GRANULAR_CANDIDATES
 if(counts[i]>=0){for(int k=0;k<counts[i];++k){int j=candidates[candidate_index(i,k,n,capacity)];if(i!=j)force+=cohesive_force(pos[i],pos[j],r);}}else
#endif
 for(int z=-1;z<=1;++z)for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){int3 a=q+(int3)(x,y,z);if(any(a<(int3)(0))||a.x>=nx||a.y>=ny||a.z>=nz)continue;for(int j=head[(a.z*ny+a.y)*nx+a.x];j>=0;j=link[j])if(i!=j)force+=cohesive_force(pos[i],pos[j],r);}}
 if(WALL_GAMMA>0){float3 lo=(float3)(-.8f,-.5f,-.5f),hi=-lo;float magnitude=12.56637061435917f*WALL_GAMMA*ri;for(int k=0;k<3;++k){if(p[k]-lo[k]-ri<=ri*1e-5f)force[k]-=magnitude;if(hi[k]-p[k]-ri<=ri*1e-5f)force[k]+=magnitude;}}
 float3 result=force/mi;if(!all(isfinite(result))){atomic_or(error,4);return;}acceleration[i]=(float4)(result,0);
}
#endif
__kernel void inspect_boundaries(__global const float4* points,int n,float r,float angle,int gate,__global const float4* bounds,
 __global float4* projected,__global float4* normals,__global float4* velocities){
 int i=get_global_id(0);if(i>=n)return;
#if GRANULAR_BINARY
 r=grain_radius(points[i].w,r)*(r/fmax(MAT_R0,MAT_R1));
#endif
 float3 wn,wv;int work[2]={0,0};float3 p=boundary_eval(points[i].xyz,r,angle,gate,&wn,&wv,bounds,work);
 projected[i]=(float4)(p,points[i].w);normals[i]=(float4)(wn,(float)work[0]);velocities[i]=(float4)(wv,(float)work[1]);
}
)CLC"
R"CLC(
// Build on current positions when required by the cache-validity check (or every iteration without reuse).
__kernel void build_candidates(__global const float4* pos,__global const int* head,__global const int* link,
 int n,float r,float cell,int nx,int ny,int nz,__global int* candidates,__global int* counts,int capacity,__global unsigned* overflow
#if GRANULAR_REUSE
 ,__global float4* reference,__global unsigned* control,float skin
#endif
 ){
#if GRANULAR_REUSE
 if(!control[0])return;
#endif
 int i=get_global_id(0);if(i>=n)return;float3 p=pos[i].xyz;int3 q=convert_int3(floor((p+GRID_OFFSET)/cell));int count=0;
 float reach=2*r;
#if GRANULAR_REUSE
 reach+=skin;reference[i]=pos[i];if(i==0)++control[3];
#endif
 for(int z=-1;z<=1;z++)for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
  int3 c=q+(int3)(x,y,z);if(any(c<(int3)(0))||c.x>=nx||c.y>=ny||c.z>=nz)continue;
  for(int j=head[(c.z*ny+c.y)*nx+c.x];j>=0;j=link[j])if(j!=i){float3 d=p-pos[j].xyz;
   // Small outward tolerance avoids rounding false negatives; solve rechecks exact contact.
   if(dot(d,d)<=reach*reach*1.00001f){if(count<capacity)candidates[candidate_index(i,count,n,capacity)]=j;++count;}
  }
 }
 counts[i]=count<=capacity?count:-1;if(count>capacity){if(atomic_inc(overflow)==0xffffffffu)atomic_inc(overflow+1);
#if GRANULAR_REUSE
 atomic_or(control+2,1u); // Force a fresh grid next iteration for overflow fallback.
#endif
 }
}
// In-order queue boundaries provide global synchronization between these kernels.
// control: rebuild-now, valid, prior-overflow, build-count, check-count, immutable-forced-rebuild.
__kernel void reuse_begin(__global unsigned* control){if(get_global_id(0)==0){control[0]=!control[1]||control[2];control[5]=control[0];control[1]=1;control[2]=0;++control[4];}}
__kernel void reuse_check(__global const float4* pos,__global const float4* reference,int n,float skin,
 __global unsigned* control,float cell,int nx,int ny,int nz,__global int* error){
 // This flag is immutable for this dispatch, avoiding N contended atomic reads.
 if(control[5])return;
 int i=get_global_id(0),lid=get_local_id(0);int need=0;
 if(i<n){float3 p=pos[i].xyz;
  if(!all(isfinite(p))){atomic_or(error,4);need=1;}
  else if(cellid(p,cell,nx,ny,nz)<0){atomic_or(error,1);need=1;}
  else{float3 d=p-reference[i].xyz;float limit=skin*.49f;need=dot(d,d)>=limit*limit;}
 }
 // run() uses exactly 128 lanes; padded lanes participate with zero.
 __local int changed[128];changed[lid]=need;barrier(CLK_LOCAL_MEM_FENCE);
 for(int stride=64;stride>0;stride/=2){if(lid<stride)changed[lid]|=changed[lid+stride];barrier(CLK_LOCAL_MEM_FENCE);}
 if(lid==0&&changed[0])atomic_or(control,1u);
}
// Optional small-scene all-pairs oracle, immediately before EVERY contact solve.
__kernel void audit_candidates(__global const float4* pos,int n,float r,__global const int* candidates,
 __global const int* counts,int capacity,__global const int* head,__global const int* link,
 float cell,int nx,int ny,int nz,__global int* error){
 int i=get_global_id(0);if(i>=n)return;
 for(int j=0;j<n;++j)if(j!=i){float3 d=pos[i].xyz-pos[j].xyz;float reach=grain_radius(pos[i].w,r)+grain_radius(pos[j].w,r);if(dot(d,d)>=reach*reach)continue;int found=0;
  if(counts[i]>=0){for(int k=0;k<counts[i];++k)if(candidates[candidate_index(i,k,n,capacity)]==j){found=1;break;}}
  else{int3 q=convert_int3(floor((pos[i].xyz+GRID_OFFSET)/cell));for(int z=-1;z<=1;z++)for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
   int3 c=q+(int3)(x,y,z);if(any(c<(int3)(0))||c.x>=nx||c.y>=ny||c.z>=nz)continue;
   for(int k=head[(c.z*ny+c.y)*nx+c.x];k>=0;k=link[k])if(k==j)found=1;
  }}
  if(!found)atomic_or(error,8);
 }
}
__kernel void solve(__global const float4* pos,__global const float4* old,__global float4* next,__global const int* head,__global const int* link,int n,float r,float cell,int nx,int ny,int nz,float angle,int gate,float mu
#if GRANULAR_CANDIDATES
 ,__global const int* candidates,__global const int* counts,int capacity
#endif
 ,__global const float4* bounds,__global int* error
#if GRANULAR_ROTATION
 ,__global const float4* angular,__global float4* nextangular
#endif
#if GRANULAR_COHESION
 ,__global float4* contact_correction
#endif
 ){
 int i=get_global_id(0);if(i>=n)return;float3 ac=(float3)(0);float3 p=pos[i].xyz;int3 q=convert_int3(floor((p+GRID_OFFSET)/cell));float3 correction=(float3)(0);
#if GRANULAR_CANDIDATES
 if(counts[i]>=0){for(int k=0;k<counts[i];++k){int j=candidates[candidate_index(i,k,n,capacity)];if(j!=i){correction+=pair_correction(pos[i],pos[j],old[i].xyz,old[j].xyz,i,j,r,mu
#if GRANULAR_ROTATION
 ,angular[i].xyz,angular[j].xyz,&ac
#endif
 );}}}
 else
#endif
 { // On overflow traverse the entire original grid; never consume a truncated list.
 for(int z=-1;z<=1;z++)for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
  int3 c=q+(int3)(x,y,z);
#if GRANULAR_PLANT
  int j=head[plant_hash(c)];while(j>=0){if(j!=i&&all(convert_int3(floor((pos[j].xyz+GRID_OFFSET)/cell))==c)){
#else
  if(any(c<(int3)(0))||c.x>=nx||c.y>=ny||c.z>=nz)continue;
  int j=head[(c.z*ny+c.y)*nx+c.x];while(j>=0){if(j!=i){
#endif
correction+=pair_correction(pos[i],pos[j],old[i].xyz,old[j].xyz,i,j,r,mu
#if GRANULAR_ROTATION
 ,angular[i].xyz,angular[j].xyz,&ac
#endif
 );}j=link[j];}
 }
 }
 r=grain_radius(pos[i].w,r);float l=length(correction);
#if GRANULAR_BINARY
 // Independent per-particle clipping would destroy mass-weighted momentum and
 // angular consistency. Reject through the finite-state guard instead.
 if(l>r*.35f){atomic_or(error,16);correction=(float3)(0);ac=(float3)(0);}
#else
 if(l>r*.35f)correction*=r*.35f/l;
#endif
#if GRANULAR_PROFILE_SPLIT
 p+=correction;
#else
 float3 wn,wv;float3 unbounded=p+correction;
#if GRANULAR_PLANT
 p=plant_boundary_ref(unbounded,r,angle,bounds,&wn,&wv,old[i].xyz);
#else
 p=boundaries(unbounded,r,angle,gate,&wn,&wv,bounds);
#endif
#if GRANULAR_COHESION
 contact_correction[i].xyz+=correction+(p-unbounded);
#endif
#endif
 next[i]=(float4)(p,pos[i].w);
#if GRANULAR_ROTATION
 nextangular[i]=(float4)(angular[i].xyz+ac,0);
#endif
}
__kernel void velocity(__global float4* pos,__global const float4* old,__global float4* vel,int n,float dt,float r,float angle,float omega,int gate,float mu,__global int* error,__global const float4* predicted
#if GRANULAR_PROFILE_SPLIT
 ,__global const float4* normals,__global const float4* wallvelocities
#endif
 ,__global const float4* bounds
#if GRANULAR_ROTATION
 ,__global const float4* angular,__global float4* spin
#endif
#if GRANULAR_COHESION
 ,__global const float4* contact_correction
#endif
 ){
 int i=get_global_id(0);if(i>=n)return;r=grain_radius(pos[i].w,r);mu=grain_wall_mu(pos[i].w,mu);float3 p=pos[i].xyz;float3 v=vel[i].xyz+(p-predicted[i].xyz)/dt,wn,wv;
 #if GRANULAR_COHESION
 v=vel[i].xyz+contact_correction[i].xyz/dt;
#endif
 // Slightly inflated shell only identifies contact for dissipative wall friction.
#if GRANULAR_PROFILE_SPLIT
 wn=normals[i].xyz;wv=wallvelocities[i].xyz;
#else
#if GRANULAR_PLANT
 plant_boundary_ref(p,r*1.002f,angle,bounds,&wn,&wv,old[i].xyz);
#else
 boundaries(p,r*1.002f,angle,gate,&wn,&wv,bounds);
#endif
#endif
 wv*=omega;
#if GRANULAR_ROTATION
 float3 w=angular[i].xyz/dt;
#endif
 if(length(wn)>0){float3 relative=v-wv;float vn=dot(relative,wn);float3 tangent=relative-vn*wn;float tl=length(tangent);float normalImpulse;
#if GRANULAR_AIR || GRANULAR_COUPLED_AIR || GRANULAR_COHESION || GRANULAR_PLANT
 // Free predicted velocity includes drag and buoyancy; no extra dry gravity impulse.
 normalImpulse=fmax(0.f,-dot(vel[i].xyz-wv,wn));
#else
 normalImpulse=fmax(0.f,-dot((p-old[i].xyz)/dt,wn))+9.81f*dt;
#endif


#if GRANULAR_ROTATION
 tangent-=cross(w*r,wn);tl=length(tangent);
 if(tl>1.e-9f){float3 dv=-tangent*fmin(1.f/3.5f,mu*normalImpulse/tl);v+=dv;w-=2.5f/r*cross(wn,dv);}
 float3 roll=w-wn*dot(w,wn);float rl=length(roll);if(rl>1.e-9f)w-=roll*fmin(1.f,2.5f*MAT_ROLL*normalImpulse/(r*rl));
#else
 if(tl>1.e-9f)v-=tangent*fmin(1.f,mu*normalImpulse/tl);
#endif
 if(vn<0)v-=vn*wn;}

#if GRANULAR_ROTATION
 if(!all(isfinite(w))){atomic_or(error,4);return;}spin[i]=(float4)(w,0);
#endif
 if(!all(isfinite(v))||!all(isfinite(p))){atomic_or(error,4);return;}vel[i]=(float4)(v,0);
}
// Diagnostic split only: preserve projection order, expose the cost of boundary work.
__kernel void project_boundary(__global float4* pos,int n,float r,float angle,int gate,__global const float4* bounds){
 int i=get_global_id(0);if(i>=n)return;r=grain_radius(pos[i].w,r);float3 wn,wv;pos[i].xyz=boundaries(pos[i].xyz,r,angle,gate,&wn,&wv,bounds);
}
__kernel void probe_boundary(__global const float4* pos,int n,float r,float angle,int gate,__global float4* normals,__global float4* velocities,__global const float4* bounds){
 int i=get_global_id(0);if(i>=n)return;r=grain_radius(pos[i].w,r);float3 wn,wv;boundaries(pos[i].xyz,r*1.002f,angle,gate,&wn,&wv,bounds);normals[i]=(float4)(wn,0);velocities[i]=(float4)(wv,0);
}
)CLC"

