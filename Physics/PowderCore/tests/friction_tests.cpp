#include "powder/friction.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace powder;
constexpr double pi=3.14159265358979323846;
void check(bool v,const char* s){if(!v)throw std::runtime_error(s);}
void near(double a,double b,double tol,const char* s){check(std::abs(a-b)<=tol,s);}
Particle body(ParticleId id){Particle p;p.id=id;p.diameter_m=1;p.material.density_kg_m3=6/pi;return p;}
double energy(const Particle& p){return .5*(p.velocity_m_s.x*p.velocity_m_s.x+p.velocity_m_s.y*p.velocity_m_s.y+p.velocity_m_s.z*p.velocity_m_s.z)+.05*(p.angular_velocity_rad_s.x*p.angular_velocity_rad_s.x+p.angular_velocity_rad_s.y*p.angular_velocity_rad_s.y+p.angular_velocity_rad_s.z*p.angular_velocity_rad_s.z);}
int main(){try{
 auto p=body(1);p.angular_velocity_rad_s.z=2;
 for(int i=0;i<10000;++i){ForceAccumulator f;integrate_rigid(p,f,{},.0001);}
 near(p.orientation.w,std::cos(1.),1e-10,"free spin w");near(p.orientation.z,std::sin(1.),1e-10,"free spin z");
 near(p.angular_velocity_rad_s.z,2,1e-12,"free spin velocity");
 p=body(1);ForceAccumulator f;integrate_rigid(p,f,{0,0,.2},.01);near(p.angular_velocity_rad_s.z,.02,1e-12,"torque/inertia");
 FrictionModel m;ContactHistory h;p=body(1);p.position_m.z=.499;p.velocity_m_s.x=1;
 auto response=step_friction_plane(p,f,{0,0,1},0,m,h,1e-5);
 check(response.sliding,"Coulomb slip");near(std::abs(response.tangent_force_n.x),m.mu*response.normal_force_n,1e-12,"Coulomb cap");
 check(p.angular_velocity_rad_s.y>0,"friction torque direction");
 p.position_m.z=1;step_friction_plane(p,f,{0,0,1},0,m,h,1e-5);check(!h.active,"separation clears history");
 // Static tangential load below Coulomb limit: low slip stays in spring regime.
 p=body(1);p.position_m.z=.499;p.velocity_m_s.x=.0001;h={};
 response=step_friction_plane(p,f,{0,0,1},0,m,h,1e-5);check(!response.sliding&&h.active,"sticking spring regime");
 // Rotate a stored spring 90 degrees with the plane normal, preserving magnitude.
 p=body(1);p.position_m={.499,0,0};h={true,true,1,0,{0,0,1},{.0001,0,0}};
 step_friction_plane(p,f,{1,0,0},0,m,h,1e-5);
 near(h.displacement_m.x,0,1e-12,"history tangent plane");near(std::abs(h.displacement_m.z),.0001,1e-12,"history norm preserved");
 // Rolling limit on a horizontal plane: v=5/7 of initial translational speed.
 p=body(1);p.position_m.z=.5-9.81/m.kn;p.velocity_m_s.x=1;h={};
 for(int i=0;i<100000;++i){ForceAccumulator load;load.add_gravity(1,{0,0,-9.81});step_friction_plane(p,load,{0,0,1},0,m,h,1e-5);}
 near(p.velocity_m_s.x,5./7,.003,"slide to roll speed");
 near(p.velocity_m_s.x-p.angular_velocity_rad_s.y*p.position_m.z,0,1e-5,"contact slip relaxes");
 check(energy(p)<.5,"friction dissipates energy");
 std::cout<<"rolling v="<<p.velocity_m_s.x<<" omega="<<p.angular_velocity_rad_s.y<<" energy="<<energy(p)<<'\n';
 // Frictionless inclined plane: tangential acceleration g*sin(theta).
 const double theta=.2,s=std::sin(theta),c=std::cos(theta),dt=1e-5;
 m.mu=0;p=body(1);const double distance=.5-9.81*c/m.kn;p.position_m={s*distance,0,c*distance};h={};
 for(int i=0;i<10000;++i){ForceAccumulator load;load.add_gravity(1,{0,0,-9.81});step_friction_plane(p,load,{s,0,c},0,m,h,dt);}
 near(p.velocity_m_s.x*c-p.velocity_m_s.z*s,9.81*s*.1,1e-9,"incline acceleration");near(p.angular_velocity_rad_s.y,0,1e-12,"frictionless no torque");
 // Frictional incline, rolling acceleration tends to (5/7)*g*sin(theta).
 m.mu=.3;p=body(1);p.position_m={s*distance,0,c*distance};h={};
 for(int i=0;i<50000;++i){ForceAccumulator load;load.add_gravity(1,{0,0,-9.81});step_friction_plane(p,load,{s,0,c},0,m,h,dt);}
 const double incline_speed=p.velocity_m_s.x*c-p.velocity_m_s.z*s;
 near(incline_speed,(5./7)*9.81*s*.5,.004,"frictional incline rolling speed");
 check(p.angular_velocity_rad_s.y>0,"incline rolling direction");
 std::cout<<"incline rolling speed="<<incline_speed<<" reference="<<(5./7)*9.81*s*.5<<'\n';
 // Oblique pair: total linear and angular momentum, including orbital part.
 m.mu=.3;auto a=body(1),b=body(2);a.position_m.x=-.499;b.position_m.x=.499;
 a.velocity_m_s={1,.2,0};b.velocity_m_s={-1,-.2,0};h={};
 const double initialL=-.1996;
 double maxL=0;const double initialEnergy=energy(a)+energy(b)+.5*m.kn*.002*.002;
 for(int i=0;i<50000;++i){ForceAccumulator fa,fb;step_friction_pair(a,b,fa,fb,m,h,1e-6);
  near(a.velocity_m_s.x+b.velocity_m_s.x,0,1e-10,"pair momentum x");near(a.velocity_m_s.y+b.velocity_m_s.y,0,1e-10,"pair momentum y");
  double L=a.position_m.x*a.velocity_m_s.y-a.position_m.y*a.velocity_m_s.x+b.position_m.x*b.velocity_m_s.y-b.position_m.y*b.velocity_m_s.x+.1*(a.angular_velocity_rad_s.z+b.angular_velocity_rad_s.z);
  maxL=std::max(maxL,std::abs(L-initialL));
 }
 check(maxL<1e-9,"angular momentum conservation");check(energy(a)+energy(b)<initialEnergy,"oblique energy loss");
 check(std::abs(a.angular_velocity_rad_s.z)>0,"oblique generates spin");
 check(!h.active,"pair separation clears history");
 std::cout<<"oblique max angular momentum error="<<maxL<<'\n';
 // Invalid parameters and state must not partially update history or particle.
 p=body(1);p.position_m.z=.499;h={};auto old=p;m.mu=-1;bool rejected=false;
 try{step_friction_plane(p,f,{0,0,1},0,m,h,dt);}catch(const std::invalid_argument&){rejected=true;}
 check(rejected&&!h.active&&p.position_m.z==old.position_m.z,"atomic invalid model");
 m.mu=.3;h={true,true,99,0,{0,0,1},{}};rejected=false;
 try{step_friction_plane(p,f,{0,0,1},0,m,h,dt);}catch(const std::invalid_argument&){rejected=true;}
 check(rejected,"history identity mismatch");
 std::cout<<"PASS rotation and friction\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
