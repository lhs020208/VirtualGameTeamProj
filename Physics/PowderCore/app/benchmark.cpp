#include "powder/settings.hpp"
#include "powder/geometry.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>
using namespace powder;
constexpr double pi=3.14159265358979323846;
struct Row {
 std::string name,kind,criterion,error,state;
 std::size_t n=0,steps=0,contacts=0;
 double k=0,dt=0,time=0,wall=0,pos_error=0,vel_error=0,overlap=0,ratio=0,energy=0,mass_error=0,momentum_error=0;
 bool passed=true,analytic_valid=false,energy_valid=false,momentum_valid=false;
};
double mass(const Particle& p){return sphere_properties(p.diameter_m,p.material).mass_kg;}
double norm(Vec3 p){return std::hypot(p.x,p.y,p.z);}
double total_mass(const ParticleSystem& w){double m=0;for(auto& p:w.particles)m+=mass(p);return m;}
Vec3 momentum(const ParticleSystem& w){Vec3 v;for(auto& p:w.particles){double m=mass(p);v.x+=m*p.velocity_m_s.x;v.y+=m*p.velocity_m_s.y;v.z+=m*p.velocity_m_s.z;}return v;}
std::string state(const ParticleSystem& w){std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17);auto v=[&](Vec3 x){o<<x.x<<'/'<<x.y<<'/'<<x.z<<'/';};for(auto& p:w.particles){o<<p.id<<'/'<<p.diameter_m<<'/'<<p.material.density_kg_m3<<'/';v(p.position_m);v(p.velocity_m_s);v(p.angular_velocity_rad_s);o<<p.orientation.w<<'/'<<p.orientation.x<<'/'<<p.orientation.y<<'/'<<p.orientation.z<<';';}o<<w.active_pair_histories()<<'/'<<w.active_wall_histories();return o.str();}
Particle body(ParticleId id,Vec3 x,double d=1,double m=1){Particle p;p.id=id;p.position_m=x;p.diameter_m=d;p.material.density_kg_m3=6*m/(pi*d*d*d);return p;}
ParticleSystem linear(double k){ParticleSystem w;w.gravity_m_s2={};w.model.law=ContactLaw::Linear;w.model.linear={k,0,k/4,0,0};return w;}
void metrics(Row& r,const StepReport& s,const ParticleSystem& w,double initial_mass,Vec3 expected_momentum){
 r.overlap=std::max(r.overlap,s.max_overlap_m);double min_radius=1e100;for(auto& p:w.particles)min_radius=std::min(min_radius,p.diameter_m/2);r.ratio=std::max(r.ratio,s.max_overlap_m/min_radius);
 r.mass_error=std::max(r.mass_error,std::abs(s.total_mass_kg-initial_mass));auto p=momentum(w);r.momentum_error=std::max(r.momentum_error,norm({p.x-expected_momentum.x,p.y-expected_momentum.y,p.z-expected_momentum.z}));r.contacts+=s.pair_contacts+s.wall_contacts;
}
template<class F>void run(Row& r,ParticleSystem& w,std::size_t steps,F inspect){
 auto start=std::chrono::steady_clock::now();for(std::size_t i=0;i<steps;++i){auto s=w.step(r.dt);++r.steps;r.time=r.steps*r.dt;inspect(s);}r.wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();r.state=state(w);
}
std::string quote(std::string x){std::string s="\"";for(char c:x){if(c=='"')s+='"';s+=c;}return s+'"';}
void write(std::ostream& o,const Row& r){
 o<<quote(r.name)<<','<<quote(r.kind)<<','<<r.n<<','<<r.k<<','<<r.dt<<','<<r.steps<<','<<r.time<<','<<r.wall<<','<<(r.wall?r.steps/r.wall:0)<<','<<(r.wall?r.steps*r.n/r.wall:0)<<','<<(r.time?r.wall/r.time:0)<<',';
 if(r.analytic_valid)o<<r.pos_error;o<<',';if(r.analytic_valid)o<<r.vel_error;o<<','<<r.overlap<<','<<r.ratio<<',';if(r.energy_valid)o<<r.energy;o<<','<<r.mass_error<<',';if(r.momentum_valid)o<<r.momentum_error;
 o<<','<<r.contacts<<','<<r.passed<<','<<quote(r.criterion)<<','<<quote(r.error)<<','<<quote(r.state)<<'\n';o.flush();
}
int main(int argc,char** argv){try{
 if(argc!=3)throw std::invalid_argument("usage: powder_bench fixture.boxes output.csv");if(std::filesystem::exists(argv[2]))throw std::invalid_argument("output exists; choose new path");
 std::ifstream f(argv[1]);if(!f)throw std::runtime_error("missing rig fixture");auto rig=read_boxes(f);if(rig.size()!=53)throw std::runtime_error("expected 53 rig boxes");
 std::ofstream out(argv[2],std::ios::binary);out.imbue(std::locale::classic());out<<std::setprecision(17);out<<"case,kind,n,kn_n_m,dt_s,steps,physical_s,wall_s,steps_per_s,particle_steps_per_s,wall_s_per_physical_s,position_error_m,velocity_error_m_s,max_overlap_m,max_overlap_radius_ratio,max_relative_energy_error,max_mass_error_kg,max_momentum_error_kg_m_s,contact_observations,passed,criterion,error,final_state\n";
 bool all=true;auto emit=[&](Row& r){write(out,r);all=all&&r.passed;std::cout<<r.name<<" "<<(r.passed?"PASS":"FAIL")<<" wall="<<r.wall<<"s physical="<<r.time<<"s\n";std::cout.flush();};
 // Conservative elastic pair: delta=sin(sqrt(2k)*t)/sqrt(2k), relative impact speed 1.
 for(double k:{1e3,1e4,1e5})for(int steps:{200,400,800}){
  Row r;r.name="pair_k"+std::to_string(int(k))+"_s"+std::to_string(steps);r.kind="pair_convergence";r.analytic_valid=r.energy_valid=r.momentum_valid=true;r.n=2;r.k=k;r.criterion="pair_analytic_v1";double omega=std::sqrt(2*k);r.dt=(pi/(4*omega))/steps;
  auto w=linear(k);w.particles={body(1,{-.5,0,0}),body(2,{.5,0,0})};w.particles[0].velocity_m_s.x=.5;w.particles[1].velocity_m_s.x=-.5;
  run(r,w,steps,[&](auto s){metrics(r,s,w,2,{});double d=std::max(0.,1-(w.particles[1].position_m.x-w.particles[0].position_m.x));double e=.5*(std::pow(w.particles[0].velocity_m_s.x,2)+std::pow(w.particles[1].velocity_m_s.x,2))+.5*k*d*d;r.energy=std::max(r.energy,std::abs(e-.25)/.25);});
  r.pos_error=std::abs(w.particles[0].position_m.x-(-.5+std::sin(omega*r.time)/(2*omega)));r.vel_error=std::abs(w.particles[0].velocity_m_s.x-.5*std::cos(omega*r.time));r.passed=r.pos_error<1e-5&&r.vel_error<.001&&r.energy<.005&&r.mass_error<1e-12&&r.momentum_error<1e-12&&r.contacts>0;emit(r);
 }
 // Actual collector base contact, conservative undamped oscillator, initially compressed 10 micrometres.
 for(double k:{4e5,4e6})for(int steps:{200,400,800}){
  Row r;r.name="rig_floor_k"+std::to_string(int(k))+"_s"+std::to_string(steps);r.kind="rig_floor_convergence";r.analytic_valid=r.energy_valid=true;r.n=1;r.k=k;r.criterion="floor_analytic_v1";
  auto w=linear(k);w.boxes=rig;double m=pi/6*.04*.04*.04*2500;double omega=std::sqrt(k/m),delta0=1e-5;r.dt=(pi/(4*omega))/steps;w.particles={body(1,{0,0,1.04+.02-delta0},.04,m)};
  run(r,w,steps,[&](auto s){metrics(r,s,w,m,{0,0,m*w.particles[0].velocity_m_s.z});double d=1.06-w.particles[0].position_m.z;double e=.5*m*std::pow(w.particles[0].velocity_m_s.z,2)+.5*k*d*d;r.energy=std::max(r.energy,std::abs(e-.5*k*delta0*delta0)/(.5*k*delta0*delta0));});
  r.pos_error=std::abs(w.particles[0].position_m.z-(1.06-delta0*std::cos(omega*r.time)));r.vel_error=std::abs(w.particles[0].velocity_m_s.z-delta0*omega*std::sin(omega*r.time));r.passed=r.pos_error<2e-8&&r.vel_error<2e-4&&r.energy<.005&&r.mass_error<1e-12&&r.contacts==r.steps;emit(r);
 }
 for(int n:{30,100,300})for(double k:{4e5,4e6})for(double dt:{2e-7,1e-7}){
  Row r;r.name="dense_n"+std::to_string(n)+"_k"+std::to_string(int(k))+"_dt"+(dt==2e-7?"2e-7":"1e-7");r.kind="dense_contact";r.momentum_valid=true;r.n=n;r.k=k;r.dt=dt;r.criterion="dense_conservation_v1";
  auto w=linear(k);for(int i=0;i<n;++i)w.particles.push_back(body(i+1,{(i%7)*.03999,((i/7)%7)*.03999,(i/49)*.03999},.04,pi/6*.04*.04*.04*2500));double initial_mass=total_mass(w);
  run(r,w,static_cast<std::size_t>(std::llround(2e-5/dt)),[&](auto s){metrics(r,s,w,initial_mass,{});});r.passed=r.mass_error<1e-10&&r.momentum_error<1e-10&&r.ratio<.01&&r.contacts>0;emit(r);
  r={};r.name="rig_initial_n"+std::to_string(n)+"_k"+std::to_string(int(k))+"_dt"+(dt==2e-7?"2e-7":"1e-7");r.kind="rig_initial_freefall";r.analytic_valid=r.momentum_valid=true;r.n=n;r.k=k;r.dt=dt;r.criterion="rig_freefall_v1";
  PowderSettings cfg;cfg.count=n;cfg.dt_s=dt;cfg.pair.linear.kn=cfg.wall.linear.kn=k;ParticleSystem geometry;geometry.boxes=rig;auto batch=make_initial(cfg,geometry);w=std::move(batch.world);auto before=w.particles;initial_mass=total_mass(w);
  run(r,w,static_cast<std::size_t>(std::llround(2e-5/dt)),[&](auto s){metrics(r,s,w,initial_mass,{0,0,-initial_mass*9.81*r.time});});
  for(std::size_t i=0;i<w.particles.size();++i){r.pos_error=std::max(r.pos_error,std::abs(w.particles[i].position_m.z-(before[i].position_m.z-.5*9.81*r.time*r.time)));r.vel_error=std::max(r.vel_error,std::abs(w.particles[i].velocity_m_s.z+9.81*r.time));}
  r.passed=r.contacts==0&&r.mass_error<1e-10&&r.momentum_error<1e-10&&r.pos_error<3e-11&&r.vel_error<1e-12;emit(r);
 }
 // Existing complete rig case: one 0.2 m, 1 kg synthetic sphere, 1.5 physical seconds.
 {
  Row r;r.name="rig_drop_1p_1.5s";r.kind="rig_full_drop";r.n=1;r.k=4e6;r.dt=2e-6;r.criterion="existing_rig_drop_v1";auto w=linear(r.k);w.gravity_m_s2={0,0,-9.81};w.model.linear={4e6,800,1e6,200,.3};w.boxes=rig;w.particles={body(1,{0,0,5.5},.2,1)};bool passed_tube=false,collector=false;
  run(r,w,750000,[&](auto s){r.overlap=std::max(r.overlap,s.max_overlap_m);r.ratio=r.overlap/.1;r.mass_error=std::max(r.mass_error,std::abs(s.total_mass_kg-1));r.contacts+=s.wall_contacts;passed_tube=passed_tube||w.particles[0].position_m.z<2.3;collector=collector||s.wall_contacts>0;});r.passed=passed_tube&&collector&&w.particles[0].position_m.z>1.04&&r.overlap<.005&&r.mass_error<1e-12;emit(r);
 }
 for(int kind=0;kind<2;++kind){Row r;r.name=kind?"reject_hertz_deep_overlap":"reject_oversized_dt";r.kind="expected_rejection";r.n=1;r.k=4e6;r.dt=kind?1e-7:1;r.criterion="atomic_rejection_v1";auto w=linear(r.k);w.planes={{1,{0,0,1},0}};w.particles={body(1,{0,0,kind?.49:.5})};if(kind){w.model.law=ContactLaw::HertzMindlin;w.model.max_overlap_radius_ratio=.01;}auto original=state(w);bool rejected=false;try{w.step(r.dt);}catch(const std::invalid_argument& e){rejected=true;r.error=e.what();}r.state=state(w);r.passed=rejected&&r.state==original;emit(r);}
 if(!out)throw std::runtime_error("cannot write benchmark output");return all?0:2;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
