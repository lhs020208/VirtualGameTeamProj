#include "powder/friction.hpp"
#include <cmath>
#include <stdexcept>
#include <algorithm>
namespace powder {
namespace {
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 mul(Vec3 a,double s){return {a.x*s,a.y*s,a.z*s};}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double norm(Vec3 a){return std::hypot(a.x,a.y,a.z);}
void scalar(double x){if(!std::isfinite(x))throw std::invalid_argument("nonfinite friction value");}
void vec(Vec3 a){scalar(a.x);scalar(a.y);scalar(a.z);}
void unit(Vec3 a){vec(a);if(std::abs(norm(a)-1)>1e-12)throw std::invalid_argument("contact normal must be unit");}
void validate_model(const FrictionModel& m,double dt,double mass,double tangential_mass){
 validate_rolling_model(m.rolling);
 scalar(m.adhesion_gamma_j_m2);if(m.adhesion_gamma_j_m2<0)throw std::invalid_argument("negative adhesion gamma");
 for(double v:{m.kn,m.cn,m.kt,m.ct,m.mu,dt,mass,tangential_mass})scalar(v);
 if(m.kn<=0||m.kt<=0||m.cn<0||m.ct<0||m.mu<0||dt<=0||mass<=0||tangential_mass<=0)throw std::invalid_argument("invalid friction parameters");
 const double rate=std::max(std::sqrt(m.kn/mass)+m.cn/mass,std::sqrt(m.kt/tangential_mass)+m.ct/tangential_mass);
 scalar(rate);if(dt>.02/rate)throw std::invalid_argument("dt exceeds normal/tangential contact limit");
}
Vec3 rotate(Vec3 v,Vec3 axis,double angle){return add(add(mul(v,std::cos(angle)),mul(cross(axis,v),std::sin(angle))),mul(axis,dot(axis,v)*(1-std::cos(angle))));}
FrictionResponse tangent(ContactHistory& h,Vec3 n,Vec3 slip,double fn,const FrictionModel& m,double dt,double spin){
 vec(slip);scalar(fn);scalar(spin);
 Vec3 x=h.displacement_m;vec(x);
 if(h.active){
  unit(h.normal);
  const Vec3 axis=cross(h.normal,n);const double sn=norm(axis),cs=std::clamp(dot(h.normal,n),-1.,1.);
  if(cs<-.999999)throw std::invalid_argument("contact normal flipped; reduce dt or reset contact");
  if(sn>1e-15)x=rotate(x,mul(axis,1/sn),std::atan2(sn,cs));
  x=rotate(x,n,spin*dt);
 }else x={};
 slip=add(slip,mul(n,-dot(slip,n)));
 x=add(add(x,mul(n,-dot(x,n))),mul(slip,dt));
 Vec3 force=add(mul(x,-m.kt),mul(slip,-m.ct));vec(force);
 const double limit=m.mu*fn;scalar(limit);const double length=norm(force);
 const bool sliding=length>limit;
 if(sliding){force=mul(force,limit/length);x=mul(add(force,mul(slip,m.ct)),-1/m.kt);}
 if(m.mu==0){force={};x={};}
 vec(x);h.displacement_m=x;h.normal=n;h.active=true;
 return {force,sliding,fn};
}
// Angular-space SDS written in length units. No twist component or net force.
Vec3 rolling(const ContactHistory& old,ContactHistory& next,Vec3 n,Vec3 relative_w,double radius,double inertia,double fn,const RollingModel& m,double dt,double spin){
 if(m.mu==0){next.rolling_displacement_m={};return {};}
 const double rate=std::sqrt(m.stiffness_n_m*radius*radius/inertia)+m.damping_ns_m*radius*radius/inertia;
 scalar(rate);if(dt>.02/rate)throw std::invalid_argument("dt exceeds rolling contact limit");
 auto h=old;h.displacement_m=old.rolling_displacement_m;
 FrictionModel pseudo; pseudo.kt=m.stiffness_n_m;pseudo.ct=m.damping_ns_m;pseudo.mu=m.mu;
 auto response=tangent(h,n,mul(relative_w,radius),fn,pseudo,dt,spin);
 next.rolling_displacement_m=h.displacement_m;
 auto torque=mul(response.tangent_force_n,radius);vec(torque);return torque;
}
void identity(const ContactHistory& h,ParticleId a,ParticleId b,bool plane){
 if(h.active&&(h.a!=a||h.b!=b||h.plane!=plane))throw std::invalid_argument("history identity mismatch");
}
}
AdhesionResponse contact_adhesion(double gamma,double radius,double overlap){
 scalar(gamma);scalar(radius);scalar(overlap);if(gamma<0||radius<=0)throw std::invalid_argument("invalid adhesion parameters");
 if(gamma==0||overlap<=0)return {};
 const double force=4*3.14159265358979323846*gamma*radius,energy=-force*overlap;scalar(force);scalar(energy);return {force,energy};
}
void validate_rolling_model(const RollingModel& m){
 for(double x:{m.stiffness_n_m,m.damping_ns_m,m.mu})scalar(x);
 if(m.stiffness_n_m<=0||m.damping_ns_m<0||m.mu<0)throw std::invalid_argument("invalid rolling parameters");
}
void integrate_rigid(Particle& p,ForceAccumulator& forces,Vec3 torque,double dt){
 validate(p);vec(torque);scalar(dt);if(dt<=0)throw std::invalid_argument("dt must be positive");
 auto next=p;auto loads=forces;
 next.angular_velocity_rad_s={};integrate_translation(next,loads,dt);
 const double inertia=sphere_properties(p.diameter_m,p.material).inertia_kg_m2;
 const Vec3 w=add(p.angular_velocity_rad_s,mul(torque,dt/inertia));vec(w);
 const double speed=norm(w),angle=speed*dt;scalar(angle);
 Quaternion d;
 if(speed>0){const double scale=std::sin(angle/2)/speed;d={std::cos(angle/2),w.x*scale,w.y*scale,w.z*scale};}
 const auto q=p.orientation;
 next.orientation={d.w*q.w-d.x*q.x-d.y*q.y-d.z*q.z,
 d.w*q.x+d.x*q.w+d.y*q.z-d.z*q.y,d.w*q.y-d.x*q.z+d.y*q.w+d.z*q.x,d.w*q.z+d.x*q.y-d.y*q.x+d.z*q.w};
 const auto r=next.orientation;const double length=std::hypot(std::hypot(r.w,r.x),std::hypot(r.y,r.z));
 next.orientation={r.w/length,r.x/length,r.y/length,r.z/length};next.angular_velocity_rad_s=w;
 validate(next);p=next;forces.clear();
}
ContactWrench evaluate_friction_plane(const Particle& p,Vec3 n,double offset,const FrictionModel& m,ContactHistory& h,double dt,Vec3 wall_velocity,Vec3 wall_omega){
 vec(wall_velocity);vec(wall_omega);
 validate(p);unit(n);scalar(offset);identity(h,p.id,0,true);
 const auto s=sphere_properties(p.diameter_m,p.material);
 const double distance=dot(n,p.position_m)-offset,overlap=s.radius_m-distance;scalar(overlap);
 if(distance<0)throw std::invalid_argument("sphere centre behind plane; reduce dt/check initial state");
 const Vec3 arm=mul(n,-distance);
 const double mt=1/(1/s.mass_kg+dot(arm,arm)/s.inertia_kg_m2);
 validate_model(m,dt,s.mass_kg,mt);
 ForceAccumulator loads;auto history=h;FrictionResponse response;Vec3 torque;
 if(overlap>0){
  const double fn=m.kn*overlap+m.cn*std::max(-dot(add(p.velocity_m_s,mul(wall_velocity,-1)),n),0.);
  const Vec3 slip=add(add(p.velocity_m_s,cross(p.angular_velocity_rad_s,arm)),mul(wall_velocity,-1));
  response=tangent(history,n,slip,fn,m,dt,dot(add(p.angular_velocity_rad_s,wall_omega),n)*.5);
  const auto adhesion=contact_adhesion(m.adhesion_gamma_j_m2,s.radius_m,overlap);
  if(adhesion.attraction_n>0&&dt>.02/std::sqrt(adhesion.attraction_n/(s.mass_kg*s.radius_m)))throw std::invalid_argument("dt exceeds adhesion curvature limit");
  loads.add(add(mul(n,fn-adhesion.attraction_n),response.tangent_force_n));torque=cross(arm,response.tangent_force_n);
  torque=add(torque,rolling(h,history,n,add(p.angular_velocity_rad_s,mul(wall_omega,-1)),s.radius_m,s.inertia_kg_m2,fn,m.rolling,dt,dot(add(p.angular_velocity_rad_s,wall_omega),n)*.5));
  history.a=p.id;history.b=0;history.plane=true;
 }else history={};
 h=history;return {response,loads.total_n(),{},torque,{}};
}
ContactWrench evaluate_friction_pair(const Particle& a,const Particle& b,const FrictionModel& m,ContactHistory& h,double dt){
 if(&a==&b)throw std::invalid_argument("pair aliases forbidden");
 identity(h,a.id,b.id,false);
 const auto contact=sphere_contact(a,b,{m.kn,m.cn});
 const auto sa=sphere_properties(a.diameter_m,a.material),sb=sphere_properties(b.diameter_m,b.material);
 const Vec3 separation=add(b.position_m,mul(a.position_m,-1));const double dist=norm(separation);const Vec3 n=mul(separation,1/dist);
 const double la=sa.radius_m-contact.overlap_m/2,lb=sb.radius_m-contact.overlap_m/2;
 if(la<=0||lb<=0)throw std::invalid_argument("excessive overlap for common contact point");
 const double mt=1/(1/sa.mass_kg+1/sb.mass_kg+la*la/sa.inertia_kg_m2+lb*lb/sb.inertia_kg_m2);
 validate_model(m,dt,contact.effective_mass_kg,mt);
 ForceAccumulator pa,pb;auto history=h;FrictionResponse response;Vec3 ta,tb;
 pa.add(contact.force_on_a_n);pb.add(contact.force_on_b_n);
 if(contact.overlap_m>0){
  const double radius=sa.radius_m/(1+sa.radius_m/sb.radius_m);
  const auto adhesion=contact_adhesion(m.adhesion_gamma_j_m2,radius,contact.overlap_m);
  if(adhesion.attraction_n>0){
   if(dt>.02/std::sqrt(adhesion.attraction_n/(contact.effective_mass_kg*std::min(sa.radius_m,sb.radius_m))))throw std::invalid_argument("dt exceeds adhesion curvature limit");
   pa.add(mul(n,adhesion.attraction_n));pb.add(mul(n,-adhesion.attraction_n));
  }
  const Vec3 ra=mul(n,la),rb=mul(n,-lb);
  const Vec3 slip=add(add(a.velocity_m_s,cross(a.angular_velocity_rad_s,ra)),mul(add(b.velocity_m_s,cross(b.angular_velocity_rad_s,rb)),-1));
  response=tangent(history,n,slip,norm(contact.force_on_a_n),m,dt,.5*dot(add(a.angular_velocity_rad_s,b.angular_velocity_rad_s),n));
  pa.add(response.tangent_force_n);pb.add(mul(response.tangent_force_n,-1));
  ta=cross(ra,response.tangent_force_n);tb=cross(rb,mul(response.tangent_force_n,-1));
  const double reff=sa.radius_m*sb.radius_m/(sa.radius_m+sb.radius_m),ieff=1/(1/sa.inertia_kg_m2+1/sb.inertia_kg_m2);
  auto tr=rolling(h,history,n,add(a.angular_velocity_rad_s,mul(b.angular_velocity_rad_s,-1)),reff,ieff,response.normal_force_n,m.rolling,dt,.5*dot(add(a.angular_velocity_rad_s,b.angular_velocity_rad_s),n));
  ta=add(ta,tr);tb=add(tb,mul(tr,-1));
  history.a=a.id;history.b=b.id;history.plane=false;
 }else history={};
 h=history;return {response,pa.total_n(),pb.total_n(),ta,tb};
}
FrictionResponse step_friction_plane(Particle& p,ForceAccumulator& f,Vec3 n,double offset,const FrictionModel& m,ContactHistory& h,double dt){
 auto next=p;auto loads=f;auto history=h;
 const auto w=evaluate_friction_plane(p,n,offset,m,history,dt);
 loads.add(w.force_a);integrate_rigid(next,loads,w.torque_a,dt);
 p=next;f.clear();h=history;return w.response;
}
FrictionResponse step_friction_pair(Particle& a,Particle& b,ForceAccumulator& fa,ForceAccumulator& fb,const FrictionModel& m,ContactHistory& h,double dt){
 if(&a==&b||&fa==&fb)throw std::invalid_argument("pair aliases forbidden");
 auto na=a,nb=b;auto pa=fa,pb=fb;auto history=h;
 const auto w=evaluate_friction_pair(a,b,m,history,dt);
 pa.add(w.force_a);pb.add(w.force_b);
 integrate_rigid(na,pa,w.torque_a,dt);integrate_rigid(nb,pb,w.torque_b,dt);
 a=na;b=nb;fa.clear();fb.clear();h=history;return w.response;
}

}
