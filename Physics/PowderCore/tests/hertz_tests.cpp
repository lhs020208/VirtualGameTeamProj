#include "powder/elastic.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace powder;
constexpr double pi=3.14159265358979323846;
void check(bool v,const char* s){if(!v)throw std::runtime_error(s);}
void near(double a,double b,double tol,const char* s){check(std::abs(a-b)<=tol,s);}
Particle body(int id){Particle p;p.id=id;p.diameter_m=1;p.material.density_kg_m3=6/pi;return p;}
struct Impact{double e,peak,time;};
Impact impact(double dt,double damping){
 auto p=body(1);p.position_m.z=.5;p.velocity_m_s.z=-.1;
 MaterialContact m;m.mu=0;m.cn=damping;ContactHistory h;double peak=0;
 for(int i=0;i<static_cast<int>(std::ceil(.05/dt));++i){auto before=p;ForceAccumulator f;step_material_plane(p,f,{0,0,1},0,m,h,dt);
  peak=std::max(peak,.5-p.position_m.z);
  if(p.position_m.z>=.5&&p.velocity_m_s.z>0){double fraction=(.5-before.position_m.z)/(p.position_m.z-before.position_m.z);return {p.velocity_m_s.z/.1,peak,(i+fraction)*dt};}
 }throw std::runtime_error("Hertz impact did not exit");
}
int main(){try{
 ElasticMaterial mat;const auto r=hertz_response(mat,mat,.5,.001);
 near(r.effective_young_pa,5333333.333333333,1e-8,"effective E");
 near(r.effective_shear_pa,1142857.1428571427,1e-8,"effective G");
 const auto doubled=hertz_response(mat,mat,.5,.002);
 near(doubled.elastic_force_n/r.elastic_force_n,std::pow(2,1.5),1e-12,"Hertz exponent");
 near(doubled.tangent_stiffness_n_m/r.tangent_stiffness_n_m,std::sqrt(2.),1e-12,"Mindlin radius scaling");
 const double eps=1e-8;
 const auto plus=hertz_response(mat,mat,.5,.001+eps),minus=hertz_response(mat,mat,.5,.001-eps);
 near((plus.elastic_force_n-minus.elastic_force_n)/(2*eps)/r.normal_derivative_n_m,1,1e-9,"force derivative");
 near((plus.elastic_energy_j-minus.elastic_energy_j)/(2*eps)/r.elastic_force_n,1,1e-9,"energy derivative");
 near(hertz_response(mat,mat,.5,0).elastic_force_n,0,0,"zero overlap");
 const double K=(4./3)*r.effective_young_pa*std::sqrt(.5),v=.1;
 const double exact_peak=std::pow(5*v*v/(4*K),.4);
 const double beta=std::tgamma(.4)*std::tgamma(.5)/std::tgamma(.9);
 const double exact_time=2*exact_peak/v*.4*beta;
 const auto coarse=impact(2e-6,0),fine=impact(2e-7,0);
 near(fine.e,1,1e-5,"elastic restitution");near(fine.peak,exact_peak,1e-8,"energy peak");near(fine.time,exact_time,2e-6,"Hertz contact time");
 check(std::abs(fine.e-1)<std::abs(coarse.e-1),"elastic dt convergence");
 const auto damp_coarse=impact(2e-6,20),damp_fine=impact(2e-7,20),reference=impact(5e-8,20);
 check(damp_fine.e<1&&damp_fine.e>0,"damped loss");
 check(std::abs(damp_fine.e-reference.e)<std::abs(damp_coarse.e-reference.e),"damped dt convergence");
 // Material plane resolves nonlinear normal force and Mindlin tangential stiffness.
 auto p=body(1);p.position_m.z=.499;p.velocity_m_s.x=.00001;ForceAccumulator f;ContactHistory h;MaterialContact m;
 auto response=step_material_plane(p,f,{0,0,1},0,m,h,1e-7);
 near(response.normal_force_n,r.elastic_force_n,1e-8,"integrated Hertz force");
 near(response.tangent_force_n.x,-r.tangent_stiffness_n_m*1e-12,1e-12,"integrated Mindlin spring");
 auto a=body(1),b=body(2);b.position_m.x=.999;ForceAccumulator fa,fb;h={};m.mu=0;
 auto pair=step_material_pair(a,b,fa,fb,m,h,1e-7);
 near(a.velocity_m_s.x+b.velocity_m_s.x,0,1e-12,"Hertz pair momentum");
 near(pair.normal_force_n,hertz_response(mat,mat,.25,.001).elastic_force_n,1e-8,"pair radius");
 m.law=ContactLaw::Linear;p=body(1);p.position_m.z=.499;auto direct=p;ContactHistory hd;h={};ForceAccumulator fd;
 step_material_plane(p,f,{0,0,1},0,m,h,1e-6);step_friction_plane(direct,fd,{0,0,1},0,m.linear,hd,1e-6);
 near(p.velocity_m_s.z,direct.velocity_m_s.z,0,"linear selection unchanged");
 for(auto bad: {ElasticMaterial{0,.25},ElasticMaterial{1e7,.5},ElasticMaterial{1e7,-1}}){bool rejected=false;try{hertz_response(bad,mat,.5,.001);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"invalid elasticity");}
 m.law=ContactLaw::HertzMindlin;p=body(1);p.position_m.z=.4;h={};bool rejected=false;
 try{step_material_plane(p,f,{0,0,1},0,m,h,1e-7);}catch(const std::invalid_argument&){rejected=true;}
 check(rejected&&p.position_m.z==.4&&!h.active,"overlap cap atomic");
 std::cout.precision(12);std::cout<<"PASS Hertz-Mindlin\nreference peak="<<exact_peak<<" time="<<exact_time<<"\nfine e="<<fine.e<<" peak="<<fine.peak<<" time="<<fine.time<<"\nelastic e errors coarse/fine="<<std::abs(coarse.e-1)<<" / "<<std::abs(fine.e-1)<<"\ndamped e coarse/fine/reference="<<damp_coarse.e<<" / "<<damp_fine.e<<" / "<<reference.e<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
