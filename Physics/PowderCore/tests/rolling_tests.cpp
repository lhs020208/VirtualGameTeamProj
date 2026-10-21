#include "powder/settings.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace powder;
constexpr double pi=3.14159265358979323846;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
void near(double a,double b,double tol,const char* s){check(std::abs(a-b)<tol,s);}
Particle body(ParticleId id,Vec3 x){Particle p;p.id=id;p.diameter_m=1;p.material.density_kg_m3=6/pi;p.position_m=x;return p;}
double norm(Vec3 v){return std::hypot(v.x,v.y,v.z);}
MaterialContact model(double mu){MaterialContact m;m.law=ContactLaw::Linear;m.linear={10000,40,2500,10,.6};m.rolling={1000,80,mu};return m;}
double kinetic(const ParticleSystem& w){double e=0;for(auto& p:w.particles)e+=.5*std::pow(norm(p.velocity_m_s),2)+.05*std::pow(norm(p.angular_velocity_rad_s),2);return e;}
struct Outcome {double x,speed,energy,peak_energy;};
Outcome slope(double mu,double dt){ParticleSystem w;w.model=model(mu);w.gravity_m_s2={9.81*std::sin(.1),0,-9.81*std::cos(.1)};w.planes={{1,{0,0,1},0}};w.particles={body(1,{0,0,.5+ w.gravity_m_s2.z/10000})};
 for(int i=0;i<int(std::llround(2/dt));++i)w.step(dt);
 auto p=w.particles[0];return {p.position_m.x,norm(p.velocity_m_s),kinetic(w),0};}
Outcome pile(double mu){ParticleSystem w;w.model=model(mu);w.planes={{1,{0,0,1},0}};
 for(int row=0;row<3;++row)for(int j=0;j<3-row;++j)w.particles.push_back(body(w.particles.size()+1,{j-1.+row*.5,0,.501+row*std::sqrt(.75)}));
 double initial=0;for(auto& p:w.particles)initial+=9.81*p.position_m.z;double peak=initial;
 for(int i=0;i<200000;++i){w.step(1e-5);double total=kinetic(w);for(auto& p:w.particles)total+=9.81*p.position_m.z;peak=std::max(peak,total);}
 double spread=0;for(auto& p:w.particles)spread=std::max(spread,std::abs(p.position_m.x));
 check(peak<initial+.03,"pile no macroscopic energy creation");check(w.active_pair_histories()>0||mu==0,"resistant pile retains pair contacts");
 return {spread,norm(w.particles.back().velocity_m_s),kinetic(w),peak-initial};}
int main(){try{
 auto m=model(.2);auto p=body(1,{0,0,.499});p.angular_velocity_rad_s={0,2,0};ContactHistory h;
 auto v=evaluate_material_plane(p,{0,0,1},0,m,h,1e-5);
 // Disable tangential force to isolate rolling wrench.
 m.linear.mu=0;h={};v=evaluate_material_plane(p,{0,0,1},0,m,h,1e-5);
 check(v.torque_a.y<0,"rolling opposes tangent rotation");near(norm(v.torque_a),1,1e-10,"mu Fn R torque cap");near(norm(v.force_a),10,1e-10,"rolling adds no force");
 p.angular_velocity_rad_s={0,0,2};h={};v=evaluate_material_plane(p,{0,0,1},0,m,h,1e-5);near(norm(v.torque_a),0,1e-14,"pure twist excluded");
 p.position_m.z=1;p.angular_velocity_rad_s={0,2,0};v=evaluate_material_plane(p,{0,0,1},0,m,h,1e-5);check(!h.active&&norm(h.rolling_displacement_m)==0,"separation clears rolling spring");
 auto a=body(1,{0,0,0}),b=body(2,{.999,0,0});a.angular_velocity_rad_s={0,2,0};h={};v=evaluate_material_pair(a,b,m,h,1e-5);
 near(v.torque_a.y+v.torque_b.y,0,1e-14,"pair opposite rolling torques");check(v.torque_a.y<0,"pair relative rotation resistance");near(norm(v.torque_a),.5,1e-10,"pair effective radius cap");
 b.angular_velocity_rad_s=a.angular_velocity_rad_s;h={};v=evaluate_material_pair(a,b,m,h,1e-5);near(norm(v.torque_a),0,1e-14,"equal spin no relative rolling");
 // Contact-frame transport preserves the rolling spring under a rotated normal.
 m=model(100);m.linear.mu=0;m.rolling.damping_ns_m=0;p=body(1,{.499,0,0});h={};h.active=true;h.plane=true;h.a=1;h.normal={0,0,1};h.rolling_displacement_m={.0001,0,0};
 evaluate_material_plane(p,{1,0,0},0,m,h,1e-5);near(h.rolling_displacement_m.x,0,1e-14,"rolling spring tangent transport");near(std::abs(h.rolling_displacement_m.z),.0001,1e-14,"rolling spring transport magnitude");
 // Both normal laws carry rolling coefficients, with independent Coulomb bounds.
 m=model(.2);m.law=ContactLaw::HertzMindlin;m.mu=0;m.cn=m.ct=0;p=body(1,{0,0,.499});p.angular_velocity_rad_s.y=2;h={};v=evaluate_material_plane(p,{0,0,1},0,m,h,1e-7);check(v.torque_a.y<0&&norm(v.torque_a)<=.2*v.response.normal_force_n*.5+1e-12,"Hertz rolling torque cap");
 // Airborne angular velocity is unaffected when rolling is enabled.
 ParticleSystem flight;flight.model=model(.2);flight.gravity_m_s2={};flight.particles={body(1,{0,0,2})};flight.particles[0].angular_velocity_rad_s={1,2,3};flight.planes={{1,{0,0,1},0}};flight.step(1e-5);near(flight.particles[0].angular_velocity_rad_s.y,2,1e-14,"no global angular damping");
 // Fixed centre/load torsional oscillator: I=0.1, K=k R^2=250; independent analytic velocity.
 double errors[2];for(int level=0;level<2;++level){double dt=level?5e-6:1e-5;p=body(1,{0,0,.499});p.angular_velocity_rad_s.y=.01;m=model(100);m.linear.mu=0;m.rolling.damping_ns_m=0;h={};double peak=0;
  for(int i=0;i<int(std::llround(.01/dt));++i){auto wrench=evaluate_material_plane(p,{0,0,1},0,m,h,dt);p.angular_velocity_rad_s.y+=wrench.torque_a.y*dt/.1;double e=.05*p.angular_velocity_rad_s.y*p.angular_velocity_rad_s.y+.5*1000*std::pow(norm(h.rolling_displacement_m),2);peak=std::max(peak,std::abs(e-5e-6)/5e-6);}
  errors[level]=std::abs(p.angular_velocity_rad_s.y-.01*std::cos(50*.01));check(peak<.001,"rolling elastic energy bound");}
 check(errors[0]/errors[1]>1.9&&errors[0]/errors[1]<2.1,"rolling oscillator dt convergence");
 // Invalid values and rolling-only timestep limits reject atomically.
 ParticleSystem w;w.model=model(.2);w.model.rolling.stiffness_n_m=1e15;w.particles={body(1,{0,0,.499})};w.planes={{1,{0,0,1},0}};bool rejected=false;try{w.step(1e-5);}catch(const std::invalid_argument&){rejected=true;}check(rejected&&w.particles[0].position_m.z==.499&&w.active_wall_histories()==0,"atomic rolling dt rejection");
 auto s=PowderSettings{};s.pair.rolling={2000,3,.1};s.wall.rolling={3000,5,.2};auto text=serialize_settings(s);auto loaded=parse_settings(text);check(serialize_settings(loaded)==text,"rolling preset roundtrip");auto world=make_initial(s,{}).world;near(world.model.rolling.mu,.1,1e-15,"pair settings applied");near(world.wall_model->rolling.mu,.2,1e-15,"wall settings applied");
 auto legacy=text.substr(0,text.find("pair_rolling_sds"));legacy.replace(legacy.find("POWDER_SETTINGS 4"),17,"POWDER_SETTINGS 2");auto old=parse_settings(legacy);check(old.pair.rolling.mu==0&&old.wall.rolling.mu==0,"legacy disables rolling");
 for(double bad:{-1.,std::numeric_limits<double>::infinity()}){s.pair.rolling.mu=bad;rejected=false;try{validate_settings(s);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"invalid rolling setting rejected");}
 auto free=slope(0,1e-5),held=slope(.2,1e-5),fine=slope(.2,5e-6);
 std::cout<<"slope_mu0 x="<<free.x<<" speed="<<free.speed<<" KE="<<free.energy<<"\nslope_mu0.2 x="<<held.x<<" speed="<<held.speed<<" KE="<<held.energy<<"\nslope_dt_half x="<<fine.x<<" speed="<<fine.speed<<'\n';
 check(free.x>1&&free.speed>1,"unresisted incline rolls");check(std::abs(held.x)<.01&&held.speed<.001,"rolling spring holds shallow incline");check(std::abs(held.x-fine.x)<1e-4,"incline dt sensitivity");
 auto loose=pile(0),resistant=pile(.2);std::cout<<"pile_mu0 spread="<<loose.x<<" KE="<<loose.energy<<" energy_excess="<<loose.peak_energy<<"\npile_mu0.2 spread="<<resistant.x<<" KE="<<resistant.energy<<" energy_excess="<<resistant.peak_energy<<'\n';
 check(resistant.energy<loose.energy&&resistant.energy<.01,"pile rolling resistance reduces residual motion");check(resistant.x<loose.x,"pile spread coefficient response");
 std::cout<<"rolling resistance PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
