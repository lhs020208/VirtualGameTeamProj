#include "powder/air.hpp"
#include <iostream>
#include <limits>
void must(bool b,const char* s){if(!b)throw std::runtime_error(s);}
template<class F> void rejects(F f){bool ok=false;try{f();}catch(const std::invalid_argument&){ok=true;}must(ok,"expected air rejection");}
int main(){try{
 powder::GranularAir a;powder::GranularMaterial m;
 must(!a.enabled,"default OFF");a.enabled=true;a.validate_material(m);
 m.source_diameter_m=1.e-4;rejects([&]{a.validate_material(m);});m.source_diameter_m=2*m.radius_m;a.validate_material(m);
 auto bad=a;bad.viscosity_pa_s=0;rejects([&]{bad.validate();});bad=a;bad.velocity_m_s[0]=std::numeric_limits<double>::quiet_NaN();rejects([&]{bad.validate();});
 const double r=.005,rho=1000;
 must(std::abs(powder::sphere_drag_beta(a,r,rho,0)-4.5*a.viscosity_pa_s/(rho*r*r))<1.e-14,"Stokes beta");
 double speed=10,mass=4.188790204786391*r*r*r*rho;
 double expected=.5*.44*a.density_kg_m3*3.141592653589793*r*r*speed/mass;
 must(std::abs(powder::sphere_drag_beta(a,r,rho,speed)-expected)<1.e-12,"Newton drag branch");
 auto stopped=powder::advance_sphere_air(a,r,rho,{0,0,0},.001,{0,0,0});must(stopped[0]==0&&stopped[2]==0,"zero slip");
 auto decel=powder::advance_sphere_air(a,r,rho,{1,0,0},.001,{0,0,0});must(decel[0]>0&&decel[0]<1,"dissipation");
 a.velocity_m_s={-1,0,0};auto moved=powder::advance_sphere_air(a,r,rho,{0,0,0},.001,{0,0,0});must(moved[0]<0&&moved[0]>-1,"air direction");
 a.velocity_m_s={0,0,0};auto neutral=powder::advance_sphere_air(a,r,a.density_kg_m3,{0,0,0},.0001);must(neutral[2]==0,"neutral buoyancy");
 auto rising=powder::advance_sphere_air(a,r,.5*a.density_kg_m3,{0,0,0},.0001);must(rising[2]>0,"buoyancy direction");
 rejects([&]{powder::advance_sphere_air(a,.0011,.001,{0,0,0},.001);});rejects([&]{powder::sphere_drag_beta(a,r,rho,101);});
 // Terminal settling: independently balance weight-buoyancy against quadratic drag.
 double lo=0,hi=50;
 for(int n=0;n<100;++n){double u=(lo+hi)/2,re=a.density_kg_m3*2*r*u/a.viscosity_pa_s;
  double cd=re<=1000?24/re*(1+.15*std::pow(re,.687)):.44;
  double force=.5*a.density_kg_m3*cd*3.141592653589793*r*r*u*u;
  if(force<mass*9.81*(1-a.density_kg_m3/rho))lo=u;else hi=u;
 }
 std::array<double,3> falling{};for(int n=0;n<30000;++n)falling=powder::advance_sphere_air(a,r,rho,falling,.001);
 must(std::abs(-falling[2]-(lo+hi)/2)<.001,"terminal settling force balance");
 a.density_kg_m3=.01;a.viscosity_pa_s=.001;
 double beta=4.5*a.viscosity_pa_s/(rho*r*r),initial=1.e-6;
 auto integrate=[&](int n){std::array<double,3> v{initial,0,0};for(int i=0;i<n;++i)v=powder::advance_sphere_air(a,r,rho,v,1./n,{0,0,0});return v[0];};
 double exact=initial*std::exp(-beta),e1=std::abs(integrate(100)-exact),e2=std::abs(integrate(200)-exact);
 must(e2<e1*.6&&e2/exact<.001,"low-Re analytic solution and timestep convergence");
 std::cout<<"PASS air contract, direct diameter, Stokes/Newton, dissipation, buoyancy, convergence, guards\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
