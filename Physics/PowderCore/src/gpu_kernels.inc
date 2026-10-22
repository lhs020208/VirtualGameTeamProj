R"CLC(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable
#pragma OPENCL FP_CONTRACT OFF
inline double4 rotation_step(double4 q,double3 w,double dt){
 double speed=length(w);double4 d=(double4)(1,0,0,0);if(speed>0){double angle=speed*dt;double f=sin(angle/2)/speed;d=(double4)(cos(angle/2),f*w.x,f*w.y,f*w.z);}
 double4 r=(double4)(d.x*q.x-d.y*q.y-d.z*q.z-d.w*q.w,d.x*q.y+d.y*q.x+d.z*q.w-d.w*q.z,d.x*q.z-d.y*q.w+d.z*q.x+d.w*q.y,d.x*q.w+d.y*q.z-d.z*q.y+d.w*q.x);return r/length(r);
}
__kernel void free_step(__global const double4* p,__global const double4* v,__global const double4* w,__global const double4* q,__global double4* pn,__global double4* vn,__global double4* wn,__global double4* qn,uint n,double dt,uint steps,double4 gravity,double4 force,__global int* error){
 uint i=get_global_id(0);if(i>=n)return;double4 x=p[i],u=v[i],omega=w[i],rotation=q[i];
 for(uint s=0;s<steps;++s){u.xyz+=(gravity.xyz+force.xyz/u.w)*dt;x.xyz+=u.xyz*dt;rotation=rotation_step(rotation,omega.xyz,dt);}
 if(!all(isfinite(x))||!all(isfinite(u))||!all(isfinite(rotation))){atomic_or(error,1);return;}pn[i]=x;vn[i]=u;wn[i]=omega;qn[i]=rotation;
}
__kernel void commit_state(__global double4* p,__global double4* v,__global double4* w,__global double4* q,__global const double4* pn,__global const double4* vn,__global const double4* wn,__global const double4* qn,uint n,__global const int* error){uint i=get_global_id(0);if(i>=n||*error)return;p[i]=pn[i];v[i]=vn[i];w[i]=wn[i];q[i]=qn[i];}
__kernel void spatial_keys(__global const double4* p,__global ulong2* keys,uint n,uint padded,double cell,__global int* error){
 uint i=get_global_id(0);if(i>=padded)return;if(i>=n){keys[i]=(ulong2)(~(ulong)0,i);return;}double3 c=floor(p[i].xyz/cell);if(!all(isfinite(c))||any(c < -1048576.)||any(c > 1048575.)){atomic_or(error,2);return;}ulong3 u=convert_ulong3(c+1048576.);keys[i]=(ulong2)(u.x|(u.y<<21)|(u.z<<42),i);
}
__kernel void sort_keys(__global ulong2* keys,uint j,uint k,uint padded){uint i=get_global_id(0),other=i^j;if(i>=padded||other<=i)return;ulong2 a=keys[i],b=keys[other];int less=a.x<b.x||(a.x==b.x&&a.y<b.y);if(less==((i&k)!=0)){keys[i]=b;keys[other]=a;}}
inline uint lower_key(__global const ulong2* keys,uint n,ulong key){uint low=0,high=n;while(low<high){uint mid=low+(high-low)/2;if(keys[mid].x<key)low=mid+1;else high=mid;}return low;}
inline ulong cell_key(long3 cell){ulong3 c=convert_ulong3(cell+(long3)(1048576));return c.x|(c.y<<21)|(c.z<<42);}
inline double3 rotate_vector(double3 v,double3 axis,double angle){double c=cos(angle),s=sin(angle);return v*c+cross(axis,v)*s+axis*dot(axis,v)*(1-c);}
inline double3 tangential(double3 n,double3 slip,double fn,double kt,double ct,double mu,double dt,double spin,double4 oldx,double4 oldn,int active,__private double3* next,__global int* error){
 double3 x=oldx.xyz;
 if(active){double3 axis=cross(oldn.xyz,n);double sn=length(axis),cs=clamp(dot(oldn.xyz,n),-1.,1.);if(cs<-.999999){atomic_or(error,64);return (double3)(0);}if(sn>1e-15)x=rotate_vector(x,axis/sn,atan2(sn,cs));x=rotate_vector(x,n,spin*dt);}else x=(double3)(0);
 slip-=n*dot(slip,n);x=x-n*dot(x,n)+slip*dt;double3 f=-kt*x-ct*slip;double limit=mu*fn,len=length(f);if(len>limit){f*=limit/len;x=-(f+ct*slip)/kt;}if(mu==0){f=(double3)(0);x=(double3)(0);}*next=x;return f;
}
__kernel void zero_hist(__global ulong* ids,__global double4* hx,__global double4* hn,__global ulong* ids2,__global double4* hx2,__global double4* hn2,uint count){uint i=get_global_id(0);if(i>=count)return;ids[i]=0;ids2[i]=0;hx[i]=(double4)(0);hn[i]=(double4)(0);hx2[i]=(double4)(0);hn2[i]=(double4)(0);}
inline double4 quat_mul(double4 a,double4 b){return (double4)(a.x*b.x-dot(a.yzw,b.yzw),a.x*b.yzw+b.x*a.yzw+cross(a.yzw,b.yzw));}
inline double3 quat_rotate(double4 q,double3 v){return v+2*cross(q.yzw,cross(q.yzw,v)+q.x*v);}
inline int wall_hit(double4 particle,__global const double4* walls,uint index,double time,__private double3* point,__private double3* normal,__private double3* velocity,__private double3* omega,__private double3* pivot,__private uint* feature,__global int* error){
 __global const double4* b=walls+index*6;int kind=(int)b[0].w;*omega=b[3].xyz;*pivot=b[2].xyz+b[4].xyz*time;
 if(kind==0){*normal=b[0].xyz;double distance=dot(*normal,particle.xyz)-b[1].w;if(distance<0){atomic_or(error,256);return -1;}*point=particle.xyz-*normal*distance;*velocity=(double3)(0);*feature=0;return distance<particle.w;}
 double4 motion=rotation_step((double4)(1,0,0,0),*omega,time),rotation=quat_mul(motion,b[5]);double3 centre=*pivot+quat_rotate(motion,b[0].xyz-b[2].xyz);double4 inverse=(double4)(rotation.x,-rotation.yzw);double3 local_position=quat_rotate(inverse,particle.xyz-centre),half_size=b[1].xyz,closest=clamp(local_position,-half_size,half_size);int count=0;uint face=0;
 if(fabs(local_position.x)>half_size.x){++count;face=local_position.x>0?1:0;}if(fabs(local_position.y)>half_size.y){++count;face=local_position.y>0?3:2;}if(fabs(local_position.z)>half_size.z){++count;face=local_position.z>0?5:4;}
 *point=centre+quat_rotate(rotation,closest);double3 delta=particle.xyz-*point;double distance=length(delta);if(count==0||distance<1e-14){atomic_or(error,256);return -1;}*normal=delta/distance;*velocity=b[4].xyz+cross(*omega,*point-*pivot);*feature=count==1?face:6;return distance<particle.w;
}
__kernel void zero_wall_load(__global double4* f,__global double4* t,__global double4* fn,__global double4* tn,uint n){uint i=get_global_id(0);if(i<n){f[i]=(double4)(0);t[i]=(double4)(0);fn[i]=(double4)(0);tn[i]=(double4)(0);}}

__kernel void contact_step(__global const double4* p,__global const double4* v,__global const double4* w,__global const double4* q,__global double4* pn,__global double4* vn,__global double4* wn,__global double4* qn,
 __global const ulong* ids,__global const ulong2* keys,__global const ulong* oldids,__global const double4* oldx,__global const double4* oldn,__global ulong* newids,__global double4* newx,__global double4* newn,__global double4* diag,
 uint n,uint slots,uint candidates,double cell,double dt,double minradius,double4 normal_model,double4 tangent_model,double4 gravity,__global int* error,double global_mobility,__global const double4* walls,uint wall_count,double time,__global const double4* old_load_f,__global double4* new_load_f,__global double4* new_load_t){
 uint i=get_global_id(0);if(i>=n||*error)return;double4 pi=p[i],vi=v[i],wi=w[i];double3 force=gravity.xyz*vi.w,torque=(double3)(0);uint contacts=0,scanned=0;double maxoverlap=0;
 for(uint h=0;h<slots;++h)newids[i*slots+h]=0;
 long3 base=convert_long3(floor(pi.xyz/cell));
 for(int dz=-1;dz<=1;++dz)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){long3 c=base+(long3)(dx,dy,dz);if(any(c<(long3)(-1048576))||any(c>(long3)(1048575)))continue;ulong key=cell_key(c);uint first=lower_key(keys,n,key);
  for(uint k=first;k<n&&keys[k].x==key;++k){uint j=(uint)keys[k].y;if(j==i)continue;if(++scanned>candidates){atomic_or(error,16);return;}double4 pj=p[j],vj=v[j],wj=w[j];double3 sep=pj.xyz-pi.xyz;double dist=length(sep),overlap=pi.w+pj.w-dist;if(overlap<=0)continue;if(dist<=0||overlap>=2*fmin(pi.w,pj.w)){atomic_or(error,4);return;}if(contacts>=slots){atomic_or(error,8);return;}
   double3 normal=sep/dist;double la=pi.w-overlap/2,lb=pj.w-overlap/2;double fn=normal_model.x*overlap+normal_model.y*fmax(dot(vi.xyz-vj.xyz,normal),0.);double3 slip=vi.xyz+cross(wi.xyz,normal*la)-vj.xyz-cross(wj.xyz,-normal*lb);
   int found=-1;for(uint h=0;h<slots;++h)if(oldids[i*slots+h]==ids[j]){found=(int)(i*slots+h);break;}double3 history;
   double3 ft=tangential(normal,slip,fn,tangent_model.x,tangent_model.y,tangent_model.z,dt,.5*dot(wi.xyz+wj.xyz,normal),found>=0?oldx[found]:(double4)(0),found>=0?oldn[found]:(double4)(0),found>=0,&history,error);
   force+=-normal*fn+ft;torque+=cross(normal*la,ft);uint h=i*slots+contacts;newids[h]=ids[j];newx[h]=(double4)(history,0);newn[h]=(double4)(normal,0);++contacts;maxoverlap=fmax(maxoverlap,overlap);
  }
 }
 double3 reaction=(double3)(0),reaction_torque=(double3)(0);double power=0;uint pair_count=contacts;
 for(uint wi_index=0;wi_index<wall_count;++wi_index){double3 point,normal,wall_velocity,wall_omega,pivot;uint feature=0;int hit=wall_hit(pi,walls,wi_index,time,&point,&normal,&wall_velocity,&wall_omega,&pivot,&feature,error);if(hit<0)return;if(!hit)continue;if(contacts>=slots){atomic_or(error,8);return;}
  double distance=length(pi.xyz-point),overlap=pi.w-distance;double3 arm=-normal*distance;double fn=normal_model.x*overlap+normal_model.y*fmax(-dot(vi.xyz-wall_velocity,normal),0.);double3 slip=vi.xyz+cross(wi.xyz,arm)-wall_velocity;ulong wall_id=((ulong)1<<63)+(wi_index+1)*8+feature;int found=-1;
  for(uint h=0;h<slots;++h)if(oldids[i*slots+h]==wall_id){found=(int)(i*slots+h);break;}double3 history;double3 ft=tangential(normal,slip,fn,tangent_model.x,tangent_model.y,tangent_model.z,dt,.5*dot(wi.xyz+wall_omega,normal),found>=0?oldx[found]:(double4)(0),found>=0?oldn[found]:(double4)(0),found>=0,&history,error);double3 load=normal*fn+ft;force+=load;torque+=cross(arm,ft);uint h=i*slots+contacts;newids[h]=wall_id;newx[h]=(double4)(history,0);newn[h]=(double4)(normal,0);++contacts;maxoverlap=fmax(maxoverlap,overlap);
  if((int)walls[wi_index*6].w==2){reaction-=load;reaction_torque-=cross(point-pivot,load);power+=dot(load,wall_velocity);}
 }
 new_load_f[i]=(double4)(reaction,old_load_f[i].w+power*dt);new_load_t[i]=(double4)(reaction_torque,power);

 double mobility=global_mobility,count=fmax(1.,(double)contacts);double rate=sqrt(2*count*fmax(normal_model.x,tangent_model.x)*mobility)+2*count*fmax(normal_model.y,tangent_model.y)*mobility;double limit=.02/rate;
 if(dt>limit){atomic_or(error,32);return;}double4 velocity=(double4)(vi.xyz+force*(dt/vi.w),vi.w);double4 position=(double4)(pi.xyz+velocity.xyz*dt,pi.w);double4 omega=(double4)(wi.xyz+torque*(dt/wi.w),wi.w);double4 orientation=rotation_step(q[i],omega.xyz,dt);
 if(!all(isfinite(position))||!all(isfinite(velocity))||!all(isfinite(omega))||!all(isfinite(orientation))){atomic_or(error,1);return;}if(length(position.xyz-pi.xyz)>.1*minradius){atomic_or(error,128);return;}
 pn[i]=position;vn[i]=velocity;wn[i]=omega;qn[i]=orientation;diag[i]=(double4)(pair_count,maxoverlap,limit,contacts-pair_count);
}
inline double3 wall_local(double3 p,__global const double4* b,double time){double4 motion=rotation_step((double4)(1,0,0,0),b[3].xyz,time),rotation=quat_mul(motion,b[5]);double3 pivot=b[2].xyz+b[4].xyz*time,centre=pivot+quat_rotate(motion,b[0].xyz-b[2].xyz);return quat_rotate((double4)(rotation.x,-rotation.yzw),p-centre);}
inline int wall_crossing(double3 before,double3 after,__global const double4* b,double time,double dt){if((int)b[0].w==0)return 0;double3 a=wall_local(before,b,time-dt),z=wall_local(after,b,time),delta=z-a;double angle=length(b[3].xyz)*dt;double pad=.5*(angle*angle*fmax(length(a),length(z))+2*angle*(length(after-before)+length(b[4].xyz)*dt));double3 half_size=b[1].xyz+(double3)(pad);double enter=0,leave=1;for(uint axis=0;axis<3;++axis){if(fabs(delta[axis])<1e-30){if(fabs(a[axis])>half_size[axis])return 0;}else{double x=(-half_size[axis]-a[axis])/delta[axis],y=(half_size[axis]-a[axis])/delta[axis];enter=fmax(enter,fmin(x,y));leave=fmin(leave,fmax(x,y));if(enter>leave)return 0;}}return 1;}
__kernel void guard_pairs(__global const double4* p,__global const ulong2* keys,uint n,double cell,uint candidates,__global int* error,__global const double4* walls,uint wall_count,double time,__global const double4* oldp,double dt){uint i=get_global_id(0);if(i>=n||*error)return;for(uint wall=0;wall<wall_count;++wall){double3 point,normal,velocity,omega,pivot;uint feature;if(wall_crossing(oldp[i].xyz,p[i].xyz,walls+wall*6,time,dt)){atomic_or(error,256);return;}wall_hit(p[i],walls,wall,time,&point,&normal,&velocity,&omega,&pivot,&feature,error);}if(*error)return;long3 base=convert_long3(floor(p[i].xyz/cell));uint scanned=0;for(int dz=-1;dz<=1;++dz)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){long3 c=base+(long3)(dx,dy,dz);if(any(c<(long3)(-1048576))||any(c>(long3)(1048575)))continue;ulong key=cell_key(c);for(uint k=lower_key(keys,n,key);k<n&&keys[k].x==key;++k){uint j=(uint)keys[k].y;if(i==j)continue;if(++scanned>candidates){atomic_or(error,16);return;}double distance=length(p[j].xyz-p[i].xyz);if(p[i].w+p[j].w-distance>=2*fmin(p[i].w,p[j].w)){atomic_or(error,4);return;}}}}
__kernel void commit_contact(__global double4* p,__global double4* v,__global double4* w,__global double4* q,__global const double4* pn,__global const double4* vn,__global const double4* wn,__global const double4* qn,
 __global ulong* ids,__global double4* hx,__global double4* hn,__global const ulong* ids2,__global const double4* hx2,__global const double4* hn2,uint n,uint slots,__global const int* error,__global ulong* accepted,__global double4* wall_f,__global double4* wall_t,__global const double4* wall_fn,__global const double4* wall_tn){uint i=get_global_id(0);if(*error)return;if(i<n){p[i]=pn[i];v[i]=vn[i];w[i]=wn[i];q[i]=qn[i];wall_f[i]=wall_fn[i];wall_t[i]=wall_tn[i];}if(i<n*slots){ids[i]=ids2[i];hx[i]=hx2[i];hn[i]=hn2[i];}if(i==0)++*accepted;}

)CLC"
