R"CLC(
#if GRANULAR_PLANT && PLANT_POWERED_VALVES
// SI Blender world X; the UE visual uses the reflected-Y equivalent (-world-X).
float3 valve_rotate(float3 p,float a){float s=sin(a),c=cos(a);return (float3)(p.x,c*p.y-s*p.z,s*p.y+c*p.z);}
float valve_distance(float3 p,float radius,float halfthickness,float3* normal){
 float radial=length(p.xy);float2 axis=radial>1.e-9f?p.xy/radial:(float2)(1,0);
 float dr=radial-radius,dz=fabs(p.z)-halfthickness;float2 outside=fmax((float2)(dr,dz),(float2)(0));
 float distance=length(outside)+fmin(fmax(dr,dz),0.f);
 if(dr>0&&dz>0)*normal=(float3)(axis*outside.x,copysign(outside.y,p.z))/length(outside);
 else if(dr>dz)*normal=(float3)(axis,0);else *normal=(float3)(0,0,copysign(1.f,p.z));
 return distance;
}
// One particle owns its force and each ledger entry: no floating atomic races.
// Soft normal spring/dashpot + Coulomb friction. Reaction is exactly -r x F.
__kernel void valve_contacts(__global const float4* pos,__global const float4* vel,__global float4* d,int n,float r,float dt,float mu,__global int* error){
 int i=get_global_id(0);if(i>=n)return;int count=(int)d[5].y,base=(int)d[5].z,scratch=(int)d[5].w;
 if(pos[i].w<0.f)r=-pos[i].w;
 float mass=4.188790204786391f*r*r*r*d[1].w;float3 acceleration=(float3)(0);
 for(int j=0;j<count;++j){int k=base+6*j;float4 g=d[(int)d[5].x+2*j],state=d[k+1],cfg=d[k+4];float3 offset=pos[i].xyz-g.xyz;
  float4 ledger=(float4)(0);
  if(dot(offset,offset)<(g.w+r+.006f)*(g.w+r+.006f)){
   float3 normal;float distance=valve_distance(valve_rotate(offset,-state.x),g.w,d[k+5].y,&normal);float penetration=r-distance;
   if(penetration>0){normal=valve_rotate(normal,state.x);float3 arm=offset-normal*r;
    float3 wall=(float3)(0,-state.y*arm.z,state.y*arm.y),relative=vel[i].xyz-wall;
    float vn=dot(relative,normal);float damping=2*cfg.z*sqrt(cfg.y*mass);
    float fn=fmax(0.f,cfg.y*penetration-damping*vn);float3 tangent=relative-vn*normal;float speed=length(tangent);
    float3 force=fn*normal;if(speed>1.e-9f)force-=tangent*(fmin(mu*fn,mass*speed/dt)/speed);
    acceleration+=force/mass;ledger=(float4)(-cross(arm,force).x,fn,penetration,0);
    // Never silently tunnel through a driven disc or conceal an invalid contact step.
    if(penetration>r*.8f)atomic_or(error,128);
   }
  }
  d[scratch+n+j*n+i]=ledger;
 }
 d[scratch+i]=(float4)(acceleration,0);
}
// One workgroup per actuator. In-order dispatch gives a complete contact reduction.
__kernel void valve_drive(__global float4* d,int n,float dt,__global int* error){
 int j=get_group_id(0),lane=get_local_id(0),k=(int)d[5].z+6*j,scratch=(int)d[5].w+n+j*n;
 float4 sum=(float4)(0);for(int i=lane;i<n;i+=128){float4 v=d[scratch+i];sum.x+=v.x;sum.y+=v.y;sum.z=fmax(sum.z,v.z);}
 __local float4 loads[128];loads[lane]=sum;barrier(CLK_LOCAL_MEM_FENCE);
 for(int stride=64;stride>0;stride/=2){if(lane<stride){loads[lane].xy+=loads[lane+stride].xy;loads[lane].z=fmax(loads[lane].z,loads[lane+stride].z);}barrier(CLK_LOCAL_MEM_FENCE);}
 if(lane)return;if(error[0])return;
 float4 cfg=d[k],s=d[k+1],e=d[k+2],fault=d[k+3],contact=d[k+4],detail=d[k+5];
 float target=d[(int)d[5].x+2*j+1].y>0?1.57079632679f:0.f;
 if(target!=s.z||s.w==0){fault.x=0;fault.y=0;}s.z=target;
 float load=loads[0].x+detail.x,err=target-s.x;float cap=cfg.x*s.w;
 int powered=s.w>0&&fault.y==0&&fabs(err)>.00001f;
 float desired=copysign(fmin(cfg.z,sqrt(2*cfg.x/cfg.w*fabs(err))),err),gain=cfg.x/(.15f*cfg.z);
 // Implicit bounded torque/speed servo; power cap is mechanical eta*P, not P/0.
 float lo=s.y-dt*(cfg.x+contact.w+fabs(load))/cfg.w,hi=s.y+dt*(cfg.x+contact.w+fabs(load))/cfg.w;
 float motor=0,omega=s.y;for(int it=0;it<30;++it){omega=(lo+hi)*.5f;
  if(powered){float limit=fmin(cap,.35f*cfg.y*s.w/fmax(fabs(omega),1.e-6f));motor=clamp(gain*(desired-omega),-limit,limit);}
  else motor=clamp(-gain*omega,-contact.w,contact.w); // finite passive gearbox holding torque
  float residual=cfg.w*(omega-s.y)/dt-motor-load;if(residual>0)hi=omega;else lo=omega;
 }
 if(!powered&&fabs(s.y)<cfg.z*.001f&&fabs(load)<=contact.w){omega=0;motor=-load;}
 float angle=s.x+omega*dt;
 if(angle<=0){angle=0;omega=fmax(0.f,omega);}if(angle>=1.57079632679f){angle=1.57079632679f;omega=fmin(0.f,omega);}
 if(powered&&fabs(err)>.02f&&fabs(omega)<cfg.z*.02f&&fabs(motor)>cap*.95f)fault.x+=dt;else fault.x=0;
 if(fault.x>=fault.z)fault.y=1;
 float watts=powered?fmin(cfg.y*s.w,cfg.y*s.w*(.2f+.8f*pown(fabs(motor)/fmax(cap,1.e-6f),2))+fmax(0.f,motor*omega)/.35f):0.f;
 if(!isfinite(angle)||!isfinite(omega)||fabs(omega)*dt>.01f){atomic_or(error,256);return;}
 s.x=angle;s.y=omega;e=(float4)(motor,loads[0].x,watts,e.w+watts*dt);detail.z=loads[0].y;detail.w=loads[0].z;
 d[k+1]=s;d[k+2]=e;d[k+3]=fault;d[k+5]=detail;
}
#endif
)CLC"
