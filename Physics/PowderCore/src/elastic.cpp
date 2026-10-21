#include "powder/elastic.hpp"
#include <cmath>
#include <algorithm>
#include <stdexcept>
namespace powder {
namespace {
void finite(double v){if(!std::isfinite(v))throw std::invalid_argument("nonfinite elastic parameter/result");}
void positive(double v){finite(v);if(v<=0)throw std::invalid_argument("elastic parameter/result must be positive");}
void material(ElasticMaterial m){positive(m.young_pa);finite(m.poisson);if(m.poisson<=-1||m.poisson>=.5)throw std::invalid_argument("Poisson ratio must be in (-1,0.5)");}
double distance(Vec3 a,Vec3 b){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);}
void configuration(const MaterialContact& m){
 if(m.law!=ContactLaw::HertzMindlin)throw std::invalid_argument("unknown contact law");
 material(m.a);material(m.b);
 for(double x:{m.cn,m.ct,m.mu}){finite(x);if(x<0)throw std::invalid_argument("negative Hertz damping/friction");}
 positive(m.max_overlap_radius_ratio);
 if(m.max_overlap_radius_ratio>.05)throw std::invalid_argument("Hertz overlap limit must not exceed 5% of radius");
}
FrictionModel resolve(const MaterialContact& m,double radius,double overlap,double cap,double mass,double mt,double dt){
 configuration(m);positive(dt);positive(mass);positive(mt);
 if(overlap>cap)throw std::invalid_argument("Hertz small-overlap limit exceeded");
 const auto bound=hertz_response(m.a,m.b,radius,cap);
 const double rate=std::max(std::sqrt(bound.normal_derivative_n_m/mass)+m.cn/mass,
                          std::sqrt(bound.tangent_stiffness_n_m/mt)+m.ct/mt);
 positive(rate);if(dt>.02/rate)throw std::invalid_argument("dt exceeds Hertz cap-stiffness limit");
 // Airborne contacts have no force; use cap coefficients solely for legacy validation.
 const double delta=overlap>0 ? overlap : cap;
 const auto value=hertz_response(m.a,m.b,radius,delta);
 return {value.elastic_force_n/delta,m.cn,value.tangent_stiffness_n_m,m.ct,m.mu};
}
}
void validate_contact_model(const MaterialContact& m){
 if(m.law==ContactLaw::Linear){
  const auto k=m.linear;for(double x:{k.kn,k.cn,k.kt,k.ct,k.mu})finite(x);
  if(k.kn<=0||k.kt<=0||k.cn<0||k.ct<0||k.mu<0)throw std::invalid_argument("invalid linear material");
 }else configuration(m);
}
HertzResponse hertz_response(ElasticMaterial a,ElasticMaterial b,double radius,double delta){
 material(a);material(b);positive(radius);finite(delta);
 if(delta<0)throw std::invalid_argument("Hertz overlap must be nonnegative");
 const double e=1/((1-a.poisson*a.poisson)/a.young_pa+(1-b.poisson*b.poisson)/b.young_pa);
 const double ga=a.young_pa/(2*(1+a.poisson)),gb=b.young_pa/(2*(1+b.poisson));
 const double g=1/((2-a.poisson)/ga+(2-b.poisson)/gb);
 positive(e);positive(g);
 const double area_radius=std::sqrt(radius)*std::sqrt(delta);
 HertzResponse r{e,g,(4./3)*e*area_radius*delta,2*e*area_radius,8*g*area_radius,0};
 r.elastic_energy_j=.4*r.elastic_force_n*delta;
 for(double x:{r.elastic_force_n,r.normal_derivative_n_m,r.tangent_stiffness_n_m,r.elastic_energy_j})finite(x);
 if(delta>0){positive(r.elastic_force_n);positive(r.normal_derivative_n_m);positive(r.tangent_stiffness_n_m);positive(r.elastic_energy_j);}
 return r;
}
FrictionResponse step_material_plane(Particle& p,ForceAccumulator& f,Vec3 n,double offset,const MaterialContact& m,ContactHistory& h,double dt){
 if(m.law==ContactLaw::Linear)return step_friction_plane(p,f,n,offset,m.linear,h,dt);
 configuration(m);validate(p);
 const auto sphere=sphere_properties(p.diameter_m,p.material);
 const double projection=p.position_m.x*n.x+p.position_m.y*n.y+p.position_m.z*n.z-offset;
 const double overlap=sphere.radius_m-projection,cap=m.max_overlap_radius_ratio*sphere.radius_m;
 const double mt=sphere.mass_kg/3.5; // Conservative arm <= radius while touching.
 const auto resolved=resolve(m,sphere.radius_m,overlap,cap,sphere.mass_kg,mt,dt);
 auto next=p;auto loads=f;auto history=h;
 const auto result=step_friction_plane(next,loads,n,offset,resolved,history,dt);
 const double next_overlap=sphere.radius_m-(next.position_m.x*n.x+next.position_m.y*n.y+next.position_m.z*n.z-offset);
 if(next_overlap>cap)throw std::invalid_argument("step exceeds Hertz overlap cap; revise stiffness/load or dt");
 p=next;f.clear();h=history;return result;
}
FrictionResponse step_material_pair(Particle& a,Particle& b,ForceAccumulator& fa,ForceAccumulator& fb,const MaterialContact& m,ContactHistory& h,double dt){
 if(m.law==ContactLaw::Linear)return step_friction_pair(a,b,fa,fb,m.linear,h,dt);
 configuration(m);validate(a);validate(b);
 if(&a==&b||&fa==&fb)throw std::invalid_argument("pair aliases forbidden");
 const auto sa=sphere_properties(a.diameter_m,a.material),sb=sphere_properties(b.diameter_m,b.material);
 const double low=std::min(sa.mass_kg,sb.mass_kg),high=std::max(sa.mass_kg,sb.mass_kg),mass=low/(1+low/high);
 const double rlow=std::min(sa.radius_m,sb.radius_m),rhigh=std::max(sa.radius_m,sb.radius_m),radius=rlow/(1+rlow/rhigh);
 const double overlap=sa.radius_m+sb.radius_m-distance(a.position_m,b.position_m),cap=m.max_overlap_radius_ratio*rlow;
 const auto resolved=resolve(m,radius,overlap,cap,mass,mass/3.5,dt);
 auto na=a,nb=b;auto pa=fa,pb=fb;auto history=h;
 const auto result=step_friction_pair(na,nb,pa,pb,resolved,history,dt);
 if(sa.radius_m+sb.radius_m-distance(na.position_m,nb.position_m)>cap)throw std::invalid_argument("step exceeds Hertz pair overlap cap");
 a=na;b=nb;fa.clear();fb.clear();h=history;return result;
}
ContactWrench evaluate_material_plane(const Particle& p,Vec3 n,double offset,const MaterialContact& m,ContactHistory& h,double dt){
 if(m.law==ContactLaw::Linear)return evaluate_friction_plane(p,n,offset,m.linear,h,dt);
 configuration(m);validate(p);
 const auto sphere=sphere_properties(p.diameter_m,p.material);
 const double projection=p.position_m.x*n.x+p.position_m.y*n.y+p.position_m.z*n.z-offset;
 const double overlap=sphere.radius_m-projection,cap=m.max_overlap_radius_ratio*sphere.radius_m;
 const double mt=sphere.mass_kg/3.5; // Conservative arm <= radius while touching.
 const auto resolved=resolve(m,sphere.radius_m,overlap,cap,sphere.mass_kg,mt,dt);
 return evaluate_friction_plane(p,n,offset,resolved,h,dt);
}
ContactWrench evaluate_material_pair(const Particle& a,const Particle& b,const MaterialContact& m,ContactHistory& h,double dt){
 if(m.law==ContactLaw::Linear)return evaluate_friction_pair(a,b,m.linear,h,dt);
 configuration(m);validate(a);validate(b);
 if(&a==&b)throw std::invalid_argument("pair aliases forbidden");
 const auto sa=sphere_properties(a.diameter_m,a.material),sb=sphere_properties(b.diameter_m,b.material);
 const double low=std::min(sa.mass_kg,sb.mass_kg),high=std::max(sa.mass_kg,sb.mass_kg),mass=low/(1+low/high);
 const double rlow=std::min(sa.radius_m,sb.radius_m),rhigh=std::max(sa.radius_m,sb.radius_m),radius=rlow/(1+rlow/rhigh);
 const double overlap=sa.radius_m+sb.radius_m-distance(a.position_m,b.position_m),cap=m.max_overlap_radius_ratio*rlow;
 const auto resolved=resolve(m,radius,overlap,cap,mass,mass/3.5,dt);
 return evaluate_friction_pair(a,b,resolved,h,dt);
}

}
