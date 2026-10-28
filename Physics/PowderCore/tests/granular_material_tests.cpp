#include "powder/granular.hpp"
#include <iostream>
#include <cmath>
#include <stdexcept>
void must(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){try{
 powder::GranularConfig c;c.binary_materials=true;c.count=2;c.radius=.01f;c.dt=.00025f;
 c.materials[0].radius_m=.006;c.materials[0].density_kg_m3=900;
 c.materials[1].radius_m=.01;c.materials[1].density_kg_m3=1500;
 c.materials[0].particle_friction=0;c.materials[1].particle_friction=0;c.cross_friction=0;
 auto charge=powder::plan_binary_charge(10,.2,c.materials);
 for(int g=0;g<2;++g)must(std::abs(charge.masses[g]-10*(g==0?.2:.8))<=c.materials[g].mass_kg()*.50001,"charge rounding");
 double m0=c.materials[0].mass_kg(),m1=c.materials[1].mass_kg();
 // Isolated overlap fixture through diagnostic set_state; no walls touch it.
 std::vector<powder::Grain> p{{0,.2f,.35f,0},{.015f,.2f,.35f,1}},v(2);
 std::vector<powder::Grain> reference;
 for(int mode=0;mode<4;++mode){c.candidates=mode>0;c.candidate_reuse=mode>1;c.kernel_optimize=mode>2;c.candidate_audit=c.candidates;
  powder::GranularGPU gpu(c);gpu.set_state(p,v);gpu.advance(1,0,false);auto a=gpu.snapshot();auto velocity=gpu.velocity_snapshot();
  must(a[1].x-a[0].x>.01599,"unequal radius contact");
  must(std::abs(m0*(a[0].x-p[0].x)+m1*(a[1].x-p[1].x))<1.e-10,"mass weighted center changed");
  must(std::abs(m0*velocity[0].x+m1*velocity[1].x)<1.e-6,"pair momentum");
  if(mode==0)reference=a;else for(int i=0;i<2;++i)must(std::abs(reference[i].x-a[i].x)<1.e-7,"candidate path mismatch");
 }
 c.candidates=false;c.candidate_reuse=false;c.candidate_audit=false;c.count=1;
 // Actual group radius must control wall projection, not broadphase max radius.
 for(int g=0;g<2;++g){powder::GranularGPU gpu(c);gpu.set_state({{.3f,.2f,.001f,float(g)}},{{0,0,0,0}});gpu.advance(1,0,false);must(std::abs(gpu.snapshot()[0].z-c.materials[g].radius_m)<1.e-6,"group wall radius");}
 // Independent wall friction: same state, zero vs high friction.
 c.materials[0].wall_friction=0;c.materials[1].wall_friction=1;
 float vx[2];for(int g=0;g<2;++g){powder::GranularGPU gpu(c);gpu.set_state({{.3f,.2f,float(c.materials[g].radius_m),float(g)}},{{.1f,0,0,0}});gpu.advance(100,0,false);vx[g]=gpu.velocity_snapshot()[0].x;}
 must(vx[0]>.09&&std::abs(vx[1])<.01,"independent wall friction");
 // A sliding solid sphere on a static plane approaches v=5/7 v0,
 // omega=v/r without rolling resistance; friction converts translation to spin.
 c.rotation=true;c.materials[0].wall_friction=.4;
 powder::GranularGPU rolling(c);rolling.set_state({{.3f,.2f,.006f,0}},{{.1f,0,0,0}});rolling.advance(300,0,false);
 auto rv=rolling.velocity_snapshot()[0];auto rw=rolling.angular_velocity_snapshot()[0];
 std::cout<<"rolling vx="<<rv.x<<" wy="<<rw.y<<'\n';
 must(std::abs(rv.x-.1*5/7)<.002,"solid sphere rolling velocity");
 must(std::abs(rv.x-rw.y*.006)<.002,"rolling no-slip constraint");
 c.rolling_friction=.1f;powder::GranularGPU resistance(c);resistance.set_state({{.3f,.2f,.006f,0}},{{.1f,0,0,0}});resistance.advance(300,0,false);
 must(std::abs(resistance.velocity_snapshot()[0].x)<std::abs(rv.x),"rolling resistance must dissipate");
 c.count=2;powder::validate_granular_initial(c,{{0,.2f,.35f,0},{.017f,.2f,.35f,1}});
 bool rejected=false;try{powder::validate_granular_initial(c,p);}catch(...){rejected=true;}must(rejected,"initial unequal overlap guard");
 powder::GranularGPU guard(c);guard.set_state({{0,.2f,.35f,0},{0,.2f,.35f,1}},v);
 rejected=false;try{guard.advance(1,0,false);}catch(const std::exception& e){rejected=std::string(e.what()).find("bits=16")!=std::string::npos;}
 must(rejected,"unsafe correction must reject rather than clip unequal momentum");
 c.count=1;powder::GranularGPU diagnostic(c);
 auto boundary=diagnostic.inspect_boundaries({{.3f,.2f,.001f,0}},0,false);
 must(std::abs(boundary[0].position.z-.006)<1.e-6,"heterogeneous boundary diagnostic radius");
 std::cout<<"PASS material mass contract, charge, unequal contact/momentum, four candidate paths, group wall radius/friction, initial validation\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
