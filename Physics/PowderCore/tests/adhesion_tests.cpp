#include "powder/settings.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace powder;
constexpr double pi=3.14159265358979323846;
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
void near(double a,double b,double tolerance,const char* message){check(std::abs(a-b)<tolerance,message);}
Particle body(ParticleId id,Vec3 x){Particle p;p.id=id;p.position_m=x;p.diameter_m=1;p.material.density_kg_m3=6/pi;return p;}
MaterialContact model(double gamma){MaterialContact m;m.law=ContactLaw::HertzMindlin;m.a=m.b={1e5,.25};m.cn=m.ct=m.mu=0;m.adhesion_gamma_j_m2=gamma;return m;}
double coefficient(const MaterialContact& m,double r){return hertz_response(m.a,m.b,r,1).elastic_force_n;}
double potential(const MaterialContact& m,double r,double overlap){return (overlap>0?hertz_response(m.a,m.b,r,overlap).elastic_energy_j:0)+contact_adhesion(m.adhesion_gamma_j_m2,r,overlap).potential_j;}
struct Result{double error=0,separation=0,velocity=0;bool detached=false;};
Result pair_run(double dt,double speed,double gamma){auto m=model(gamma);auto reference=model(1/pi);double delta=std::pow(1/coefficient(reference,.25),2./3);ParticleSystem w;w.model=m;w.gravity_m_s2={};w.particles={body(1,{-(1-delta)/2,0,0}),body(2,{(1-delta)/2,0,0})};w.particles[0].velocity_m_s.x=-speed;w.particles[1].velocity_m_s.x=speed;double initial=speed*speed+potential(m,.25,delta);Result result;
 for(int i=0;i<int(std::llround(.1/dt));++i){w.step(dt);double distance=w.particles[1].position_m.x-w.particles[0].position_m.x;double kinetic=.5*(std::pow(w.particles[0].velocity_m_s.x,2)+std::pow(w.particles[1].velocity_m_s.x,2));result.error=std::max(result.error,std::abs(kinetic+potential(m,.25,1-distance)-initial));result.detached=result.detached||distance>=1;near(w.particles[0].velocity_m_s.x+w.particles[1].velocity_m_s.x,0,1e-12,"isolated pair momentum");}
 result.separation=w.particles[1].position_m.x-w.particles[0].position_m.x;result.velocity=w.particles[1].velocity_m_s.x;return result;}
double wall_run(double outward,bool box){auto m=model(1/pi);double delta=std::pow((2-1)/coefficient(m,.5),2./3);ParticleSystem w;w.model=model(0);w.wall_model=m;w.gravity_m_s2={0,0,outward};w.particles={body(1,{0,0,.5-delta})};if(box){FixedBox b;b.id=1;b.centre_m={0,0,-.5};b.half_m={2,2,.5};w.boxes={b};}else w.planes={{1,{0,0,1},0}};
 for(int i=0;i<20000;++i)w.step(1e-5);return w.particles[0].position_m.z;}
int main(){try{
 const auto m=model(1/pi);double a=coefficient(m,.25),eq=std::pow(1/a,2./3);auto p=body(1,{0,0,0}),q=body(2,{1-eq,0,0});ContactHistory h;
 auto force=evaluate_material_pair(p,q,m,h,1e-6);near(force.force_a.x,0,1e-10,"Hertz adhesive equilibrium");near(force.force_a.x+force.force_b.x,0,1e-14,"pair action reaction");
 near(contact_adhesion(1/pi,.25,eq).attraction_n,1,1e-14,"pair pull-off force convention");near(contact_adhesion(1/pi,.5,eq).attraction_n,2,1e-14,"wall effective radius");
 // Finite-difference potential derivative and force quadrature are independent checks.
 for(double delta:{eq*.1,eq*.5,eq,eq*1.5}){q.position_m.x=1-delta;h={};force=evaluate_material_pair(p,q,m,h,1e-6);double eps=1e-8;double derivative=(potential(m,.25,delta+eps)-potential(m,.25,delta-eps))/(2*eps);near(force.force_a.x,-derivative,1e-8,"force = potential gradient");}
 double integral=0;const int bins=10000;for(int i=0;i<bins;++i){double d=eq*(i+.5)/bins;integral+=(a*std::pow(d,1.5)-1)*eq/bins;}near(integral,potential(m,.25,eq),1e-10,"separation work integral");near(-potential(m,.25,eq),.6*eq,1e-12,"DMT binding energy");
 q.position_m.x=1-eq*.1;h={};force=evaluate_material_pair(p,q,m,h,1e-6);check(force.force_a.x>0,"tensile attraction before separation");q.position_m.x=1;force=evaluate_material_pair(p,q,m,h,1e-6);near(force.force_a.x,0,1e-14,"exact zero overlap is detached");check(!h.active,"separation clears contact history");q.position_m.x=1.01;force=evaluate_material_pair(p,q,m,h,1e-6);near(force.force_a.x,0,1e-14,"no finite-gap attraction");
 auto linear=m;linear.law=ContactLaw::Linear;linear.linear={10000,0,2500,0,0};q.position_m.x=.9999;h={};force=evaluate_material_pair(p,q,linear,h,1e-6);near(force.force_a.x,0,1e-10,"linear constant-attraction equilibrium");
 // Positive friction caps use the repulsive load, not a negative net normal force.
 auto friction=m;friction.mu=.4;friction.rolling.mu=.2;p.velocity_m_s.y=1;p.angular_velocity_rad_s.z=2;q.position_m.x=1-eq*.1;h={};force=evaluate_material_pair(p,q,friction,h,1e-7);check(std::isfinite(force.torque_a.z)&&force.response.normal_force_n>0,"adhesive friction remains finite and compressive-based");p.velocity_m_s={};p.angular_velocity_rad_s={};
 auto bound=pair_run(1e-5,.005,1/pi),fine=pair_run(5e-6,.005,1/pi),escape=pair_run(1e-5,.1,1/pi),off=pair_run(1e-5,.005,0);
 std::cout<<"bound_energy_error="<<bound.error<<" half_dt_error="<<fine.error<<" separation="<<bound.separation<<"\nescape_energy_error="<<escape.error<<" separation="<<escape.separation<<" off_separation="<<off.separation<<'\n';
 check(!bound.detached&&!fine.detached,"sub-binding-energy pair remains bound");check(fine.error<bound.error*.6&&bound.error<1e-6,"bound energy timestep convergence");check(escape.detached&&escape.separation>1&&escape.error<1e-5,"super-binding-energy pair detaches with bounded error");check(off.detached,"zero gamma releases pair");
 double held=wall_run(1,false),released=wall_run(3,false),box=wall_run(1,true);double wall_eq=.5-std::pow(1/coefficient(m,.5),2./3);
 near(held,wall_eq,1e-10,"wall adhesion holds outward load below pull-off");check(released>.51,"wall releases above pull-off");near(box,held,1e-10,"box wall adhesion agrees with plane");std::cout<<"wall_hold_z="<<held<<" released_z="<<released<<" box_z="<<box<<'\n';
 PowderSettings settings;settings.pair.adhesion_gamma_j_m2=.1;settings.wall.adhesion_gamma_j_m2=.2;auto text=serialize_settings(settings);check(serialize_settings(parse_settings(text))==text,"adhesion preset roundtrip");auto w=make_initial(settings,{}).world;near(w.model.adhesion_gamma_j_m2,.1,1e-14,"pair material applied");near(w.wall_model->adhesion_gamma_j_m2,.2,1e-14,"wall material applied");
 auto legacy=text.substr(0,text.find("contact_adhesion_gamma_j_m2"));legacy.replace(legacy.find("POWDER_SETTINGS 4"),17,"POWDER_SETTINGS 3");auto old=parse_settings(legacy);check(old.pair.adhesion_gamma_j_m2==0&&old.wall.adhesion_gamma_j_m2==0,"legacy disables adhesion");
 for(double value:{-1.,std::numeric_limits<double>::infinity()}){settings.pair.adhesion_gamma_j_m2=value;bool rejected=false;try{validate_settings(settings);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"invalid adhesion rejected");}
 w=ParticleSystem{};w.model=model(1e15);w.particles={body(1,{0,0,.499})};w.planes={{1,{0,0,1},0}};bool rejected=false;try{w.step(1e-5);}catch(const std::invalid_argument&){rejected=true;}check(rejected&&w.particles[0].position_m.z==.499&&w.active_wall_histories()==0,"adhesion dt atomic rejection");
 std::cout<<"contact adhesion PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
