#include "powder/world.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace powder;
constexpr double pi=3.14159265358979323846;
void check(bool v,const char* s){if(!v)throw std::runtime_error(s);}
void near(double a,double b,double tol,const char* s){check(std::abs(a-b)<=tol,s);}
Particle body(ParticleId id,Vec3 x,double diameter=1){Particle p;p.id=id;p.position_m=x;p.diameter_m=diameter;p.material.density_kg_m3=6/pi;return p;}
ParticleSystem world(){ParticleSystem w;w.model.law=ContactLaw::Linear;w.model.linear={10000,40,2500,10,.3};return w;}
void same(const ParticleSystem& a,const ParticleSystem& b){
 for(auto p:a.particles){auto it=std::find_if(b.particles.begin(),b.particles.end(),[&](auto q){return p.id==q.id;});check(it!=b.particles.end(),"id conserved");
  auto vector_same=[](Vec3 x,Vec3 y){near(x.x,y.x,1e-12,"vector x invariant");near(x.y,y.y,1e-12,"vector y invariant");near(x.z,y.z,1e-12,"vector z invariant");};
  vector_same(p.position_m,it->position_m);vector_same(p.velocity_m_s,it->velocity_m_s);vector_same(p.angular_velocity_rad_s,it->angular_velocity_rad_s);
  near(p.orientation.w,it->orientation.w,1e-12,"orientation w invariant");near(p.orientation.x,it->orientation.x,1e-12,"orientation x invariant");near(p.orientation.y,it->orientation.y,1e-12,"orientation y invariant");near(p.orientation.z,it->orientation.z,1e-12,"orientation z invariant");
 }
}
int main(){try{
 std::mt19937 rng(1707);std::uniform_real_distribution<double> pos(-8,8),size(.1,1.5);
 std::vector<Particle> cloud;
 for(int i=0;i<400;++i)cloud.push_back(body(static_cast<ParticleId>(i),{pos(rng),pos(rng),pos(rng)},size(rng)));
 // Explicit negative and exact cell boundaries; arbitrary, non-contiguous IDs.
 cloud.push_back(body(901,{-1.5,0,0},1.5));cloud.push_back(body(900,{-1.499,0,0},.1));
 auto grid=find_contacts(cloud),brute=find_contacts(cloud,true);
 check(grid.contacts==brute.contacts,"grid contacts equal all-pairs");
 check(std::adjacent_find(grid.contacts.begin(),grid.contacts.end())==grid.contacts.end(),"no duplicate pairs");
 std::cout<<"search N="<<cloud.size()<<" contacts="<<grid.contacts.size()<<" checks grid/brute="<<grid.distance_checks<<"/"<<brute.distance_checks<<'\n';
 check(grid.distance_checks<brute.distance_checks/4,"sparse broadphase reduces checks");
 auto w=world();w.gravity_m_s2={};w.particles={body(3,{-.99,0,0}),body(1,{0,0,0}),body(2,{.99,0,0})};
 w.particles[0].velocity_m_s.y=.01;w.particles[2].velocity_m_s.y=-.01;
 auto reference=w,reordered=w;std::reverse(reordered.particles.begin(),reordered.particles.end());
 for(int i=0;i<200;++i){w.step(1e-5);reference.step(1e-5,true);reordered.step(1e-5);}
 same(w,reference);same(w,reordered);
 double px=0,py=0;for(auto p:w.particles){px+=p.velocity_m_s.x;py+=p.velocity_m_s.y;}near(px,0,1e-10,"multi-contact momentum x");near(py,0,1e-10,"multi-contact momentum y");
 auto before=w;bool rejected=false;try{w.step(1);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"large world dt rejected");same(w,before);check(w.active_pair_histories()==before.active_pair_histories(),"history atomic");
 for(std::size_t i=0;i<w.particles.size();++i)w.particles[i].position_m.x=10*static_cast<double>(i);
 w.step(1e-5);check(w.active_pair_histories()==0,"lost contacts pruned");
 auto stack=world();stack.planes={{7,{0,0,1},0}};stack.particles={body(0,{0,0,.51}),body(1,{0,0,1.52}),body(2,{0,0,2.53})};
 StepReport report;double max_overlap=0;const auto start=std::chrono::steady_clock::now();
 for(int i=0;i<250000;++i){report=stack.step(2e-5);max_overlap=std::max(max_overlap,report.max_overlap_m);}
 const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 check(stack.particles.size()==3,"particle count conserved");near(report.total_mass_kg,3,1e-12,"mass conserved");
 // Three stacked weights: floor compression=3mg/k, pair compressions=2mg/k and mg/k.
 std::cout.precision(12);std::cout<<"settling diagnostic z="<<stack.particles[0].position_m.z<<","<<stack.particles[1].position_m.z<<","<<stack.particles[2].position_m.z<<" speed="<<report.max_speed_m_s<<'\n';
 near(stack.particles[0].position_m.z,.5-3*9.81/10000,2e-5,"bottom equilibrium");
 near(stack.particles[1].position_m.z,1.5-5*9.81/10000,2e-5,"middle equilibrium");
 near(stack.particles[2].position_m.z,2.5-6*9.81/10000,2e-5,"top equilibrium");
 check(report.max_speed_m_s<1e-4,"settled stack");check(max_overlap<.03,"bounded penetration");
 std::cout.precision(12);std::cout<<"stack t=5s N=3 mass="<<report.total_mass_kg<<" maxSpeed="<<report.max_speed_m_s<<" maxOverlap="<<max_overlap<<" z="<<stack.particles[0].position_m.z<<","<<stack.particles[1].position_m.z<<","<<stack.particles[2].position_m.z<<" wallSeconds="<<seconds<<'\n';
 auto refined=world();refined.planes=stack.planes;refined.particles={body(0,{0,0,.51}),body(1,{0,0,1.52}),body(2,{0,0,2.53})};
 for(int i=0;i<500000;++i)refined.step(1e-5);
 for(std::size_t i=0;i<3;++i)near(stack.particles[i].position_m.z,refined.particles[i].position_m.z,1e-6,"settled dt refinement");
 // Hertz branch: small multi-contact equilibrium-scale overlap and simultaneous update.
 auto hz=world();hz.model.law=ContactLaw::HertzMindlin;hz.gravity_m_s2={};hz.particles={body(1,{-.9999,0,0}),body(2,{0,0,0}),body(3,{.9999,0,0})};
 auto hzref=hz;for(int i=0;i<100;++i){hz.step(1e-7);hzref.step(1e-7,true);}same(hz,hzref);
 near(hz.particles[1].velocity_m_s.x,0,1e-12,"symmetric middle stays still");
 // Scaling diagnostics: no ungrounded real-time promise.
 for(int n:{100,300,1000}){std::vector<Particle> p;for(int i=0;i<n;++i)p.push_back(body(static_cast<ParticleId>(i),{(i%10)*1.1,((i/10)%10)*1.1,(i/100)*1.1}));
  auto t=std::chrono::steady_clock::now();auto r=find_contacts(p);double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();
  check(r.contacts==find_contacts(p,true).contacts,"scaled search reference");std::cout<<"search scale N="<<n<<" checks="<<r.distance_checks<<" ms="<<ms<<'\n';}
 for(int n:{27,125,300}){
  auto many=world();many.gravity_m_s2={};
  for(int i=0;i<n;++i)many.particles.push_back(body(static_cast<ParticleId>(i),{(i%7)*.999,((i/7)%7)*.999,(i/49)*.999}));
  auto t=std::chrono::steady_clock::now();StepReport r;
  for(int i=0;i<100;++i)r=many.step(1e-5);
  near(r.total_mass_kg,static_cast<double>(n),1e-10,"scaled mass conservation");check(many.particles.size()==static_cast<std::size_t>(n),"scaled count");
  double momentum=0;for(auto p:many.particles)momentum+=p.velocity_m_s.x;near(momentum,0,1e-9,"scaled momentum");
  std::cout<<"integrate scale N="<<n<<" 100stepsSeconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count()<<" maxOverlap="<<r.max_overlap_m<<'\n';
 }
 std::cout<<"PASS many-body search and deposition\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
