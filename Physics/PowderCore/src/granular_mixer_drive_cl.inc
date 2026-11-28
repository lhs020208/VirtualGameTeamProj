R"CLC(
#if GRANULAR_PLANT && PLANT_POWERED_MIXER
// State buffer: seven float4 records, then N accelerations and N reaction ledgers.
// Query the original rotating hybrid/triangle geometry, without imposing its motion.
__kernel void mixer_contacts(__global const float4* pos,__global const float4* vel,
 __global const float4* d,__global float4* motor,int n,float r,float dt,float mu,__global int* error){
 int i=get_global_id(0);if(i>=n)return;
 if(pos[i].w<0.f)r=-pos[i].w;
 float4 s=motor[1],cfg=motor[4];float3 pivot=d[1].xyz,offset=pos[i].xyz-pivot;
 float3 rotorPoint=plant_rotate(offset,-s.x)+pivot,correction=(float3)(0);
 float3 moved=d[10].x==2.f?plant_hybrid(rotorPoint,r,(int)d[12].y,(int)d[13].y,d,&correction):plant_mesh(rotorPoint,r,(int)d[0].y,d,&correction);
 float penetration=length(correction),mass=4.188790204786391f*r*r*r*d[1].w;
 float3 force=(float3)(0);float4 ledger=(float4)(0);
 if(!all(isfinite(moved))||!isfinite(penetration)){atomic_or(error,512);return;}
 if(penetration>1.e-9f){
  float3 normal=plant_rotate(correction/penetration,s.x),arm=offset-normal*r;
  float3 relative=vel[i].xyz-(float3)(0,-s.y*arm.z,s.y*arm.y);
  float vn=dot(relative,normal),fn=fmax(0.f,cfg.x*penetration-2*cfg.y*sqrt(cfg.x*mass)*vn);
  float3 tangent=relative-vn*normal;float speed=length(tangent);force=fn*normal;
  if(speed>1.e-9f)force-=tangent*(fmin(mu*fn,mass*speed/dt)/speed);
  ledger=(float4)(-cross(arm,force).x,fn,penetration,0);
  if(penetration>r*.8f)atomic_or(error,512);
 }
 motor[7+i]=(float4)(force/mass,0);motor[7+n+i]=ledger;
}
float mixer_torque(float omega,float reference,float4 rating,float fraction,int powered){
 if(!powered)return 0;
 float cap=fmin(rating.y*fraction,rating.z*rating.x*fraction/fmax(fabs(omega),1.e-6f));
 float gain=rating.y/(.1f*1.57079632679f);
 return clamp(gain*(reference-omega),-cap,cap);
}
__kernel void mixer_drive(__global float4* motor,int n,float dt,float requested,__global int* error,int stopped_by_geometry,float lower_angle,float upper_angle){
 int lane=get_local_id(0);float4 sum=(float4)(0);
 for(int i=lane;i<n;i+=128){float4 v=motor[7+n+i];sum.x+=v.x;sum.y+=v.y;sum.z=fmax(sum.z,v.z);}
 __local float4 loads[128];loads[lane]=sum;barrier(CLK_LOCAL_MEM_FENCE);
 for(int stride=64;stride>0;stride/=2){if(lane<stride){loads[lane].xy+=loads[lane+stride].xy;loads[lane].z=fmax(loads[lane].z,loads[lane+stride].z);}barrier(CLK_LOCAL_MEM_FENCE);}
 if(lane||error[0])return;
 float4 rating=motor[0],s=motor[1],energy=motor[2],fault=motor[3],cfg=motor[4],detail=motor[5],turn=motor[6];
 // A stop/power cycle acknowledges the trip. Raising the same speed does not bypass it.
 if(requested==0||s.w==0){fault.x=0;fault.y=0;}s.z=requested;
 int powered=s.w>0&&fault.y==0&&(requested!=0||s.y!=0);
 float reference=powered?detail.y+clamp(requested-detail.y,-cfg.w*dt,cfg.w*dt):0;
 float dry=detail.x+(!powered||requested==0?fault.w:0),load=loads[0].x;
 float zeroTorque=mixer_torque(0,reference,rating,s.w,powered);
 float zeroDemand=rating.w*s.y/dt+zeroTorque+load;
 float omega=0,torque=zeroTorque;
 if(fabs(zeroDemand)>dry){
  float span=dt*(rating.y+dry+fabs(load)+cfg.z*fabs(s.y))/rating.w+fabs(s.y);
  float lo=-span,hi=span;
  for(int k=0;k<32;++k){omega=(lo+hi)*.5f;torque=mixer_torque(omega,reference,rating,s.w,powered);
   float residual=rating.w*(omega-s.y)/dt-torque-load+cfg.z*omega+copysign(dry,omega);
   if(residual>0)hi=omega;else lo=omega;
  }
 }
 float advance_angle=s.x+omega*dt;
 if(stopped_by_geometry){
  float unwrapped=turn.x*6.28318530718f+advance_angle;
  if(unwrapped>upper_angle||unwrapped<lower_angle){
   float direction=copysign(1.f,omega);
   advance_angle=clamp(unwrapped,lower_angle,upper_angle)-turn.x*6.28318530718f;
   omega=0;torque=mixer_torque(0,reference,rating,s.w,powered);
   // Total reaction includes particle load plus fixed-body contact impulse.
   load=-rating.w*s.y/dt-torque+direction*dry;
  }
 }
 float cap=rating.y*s.w;
 if(powered&&fabs(requested)>.01f&&fabs(omega)<fabs(requested)*.02f&&fabs(torque)>cap*.95f)fault.x+=dt;else fault.x=0;
 if(fault.x>=fault.z)fault.y=1;
 float watts=powered?fmax(0.f,torque*omega)+(1-rating.z)*rating.x*s.w*(.1f+.9f*pown(fabs(torque)/fmax(cap,1.e-6f),2)):0;
 float a=advance_angle,turns=stopped_by_geometry?0.f:floor(a/6.28318530718f);a-=turns*6.28318530718f;turn.x+=turns;
 if(!isfinite(a)||!isfinite(omega)||fabs(omega)*dt>.01f||watts>rating.x*s.w*1.0001f){atomic_or(error,1024);return;}
 s.x=a;s.y=omega;energy=(float4)(torque,load,watts,energy.w+watts*dt);
 detail.y=reference;detail.z=loads[0].y;detail.w=loads[0].z;
 motor[1]=s;motor[2]=energy;motor[3]=fault;motor[5]=detail;motor[6]=turn;
}
#endif
)CLC"
