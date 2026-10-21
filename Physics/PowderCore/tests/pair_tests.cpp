#include "powder/core.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace powder;
constexpr double pi=3.14159265358979323846;
void check(bool ok,const char* s) { if(!ok) throw std::runtime_error(s); }
void near(double a,double b,double tolerance,const char* s) { check(std::abs(a-b)<=tolerance,s); }
Particle body(ParticleId id,double diameter,double mass,double x,double v) {
 Particle p; p.id=id; p.diameter_m=diameter; p.material.density_kg_m3=mass/(pi/6*diameter*diameter*diameter); p.position_m.x=x; p.velocity_m_s.x=v; return p;
}
double collision(double ma,double mb,double da,double db,double damping,double dt) {
 auto a=body(1,da,ma,0,1), b=body(2,db,mb,(da+db)/2,0);
 PairContact model{10000,damping}; const double initial_energy=.5*ma;
 const double reduced=ma*mb/(ma+mb);
 const double omega=std::sqrt(10000/reduced),alpha=damping/(2*reduced);
 const double wd=std::sqrt(omega*omega-alpha*alpha);
 const double expected_e=damping==0 ? 1 : std::exp(-alpha*std::atan(wd/alpha)/wd);
 double max_momentum_error=0;
 for(int i=0;i<200000;++i) {
  ForceAccumulator fa,fb; step_pair(a,b,fa,fb,model,dt);
  max_momentum_error=std::max(max_momentum_error,std::abs(ma*a.velocity_m_s.x+mb*b.velocity_m_s.x-ma));
  if(b.position_m.x-a.position_m.x>=(da+db)/2 && b.velocity_m_s.x>a.velocity_m_s.x) {
   const double e=b.velocity_m_s.x-a.velocity_m_s.x;
   near(max_momentum_error,0,1e-10,"momentum conservation");
   near(a.velocity_m_s.x,(ma-expected_e*mb)/(ma+mb),3e-4,"a impact velocity");
   near(b.velocity_m_s.x,(1+expected_e)*ma/(ma+mb),3e-4,"b impact velocity");
   const double energy=.5*ma*a.velocity_m_s.x*a.velocity_m_s.x+.5*mb*b.velocity_m_s.x*b.velocity_m_s.x;
   const double loss=.5*reduced*(1-expected_e*expected_e);
   near(initial_energy-energy,loss,3e-4,"impact energy loss");
   std::cout<<"masses="<<ma<<","<<mb<<" diameters="<<da<<","<<db<<" c="<<damping<<" dt="<<dt<<" e="<<e<<" e_error="<<std::abs(e-expected_e)<<" maxMomentumError="<<max_momentum_error<<" energyLoss="<<initial_energy-energy<<'\n';
   return std::abs(e-expected_e);
  }
 }
 throw std::runtime_error("pair did not separate");
}
int main() {
 try {
  std::cout.precision(12);
  collision(1,1,1,1,0,1e-6);
  collision(1,3,.4,.8,0,1e-6);
  const double coarse=collision(1,3,.4,.8,40,1e-5);
  const double fine=collision(1,3,.4,.8,40,1e-6);
  check(fine<coarse,"damped convergence");
  auto a=body(10,1,1,0,1),b=body(20,1,3,.9,0);
  a.position_m={0,0,0}; b.position_m={.3,.4,.5};
  PairContact model{10000,40}; auto r=sphere_contact(a,b,model), swapped=sphere_contact(b,a,model);
  near(r.effective_mass_kg,.75,1e-14,"reduced mass");
  near(r.force_on_a_n.x+r.force_on_b_n.x,0,0,"action reaction x");
  near(r.force_on_a_n.y+r.force_on_b_n.y,0,0,"action reaction y");
  near(r.force_on_a_n.z+r.force_on_b_n.z,0,0,"action reaction z");
  near(r.force_on_a_n.x,swapped.force_on_b_n.x,1e-12,"swap symmetry");
  ForceAccumulator fa,fb; const double mass=1,dt=1e-6;
  const double oldv=a.velocity_m_s.x;
  step_pair(a,b,fa,fb,model,dt);
  near(a.velocity_m_s.x-oldv,r.force_on_a_n.x/mass*dt,1e-14,"contact applied once");
  const auto old=a; fa.add({1,0,0}); b.angular_velocity_rad_s.x=1;
  bool rejected=false;
  try { step_pair(a,b,fa,fb,model,dt); } catch(const std::invalid_argument&) { rejected=true; }
  check(rejected && a.position_m.x==old.position_m.x && fa.total_n().x==1,"pair failure atomic");
  b.angular_velocity_rad_s.x=0; b.position_m=a.position_m;
  rejected=false; try { sphere_contact(a,b,model); } catch(const std::invalid_argument&) { rejected=true; }
  check(rejected,"coincident centres rejected");
  b.position_m.x+=3;
  near(sphere_contact(a,b,model).force_on_a_n.x,0,0,"separated bodies");
  rejected=false; try { step_pair(a,b,fa,fb,model,1); } catch(const std::invalid_argument&) { rejected=true; }
  check(rejected,"large step rejected");
  rejected=false; try { step_pair(a,b,fa,fa,model,dt); } catch(const std::invalid_argument&) { rejected=true; }
  check(rejected,"load alias rejected");
  b.id=a.id;
  rejected=false; try { sphere_contact(a,b,model); } catch(const std::invalid_argument&) { rejected=true; }
  check(rejected,"duplicate ids rejected");
  std::cout<<"PASS sphere pair\n"; return 0;
 } catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}
