#include "powder/settings.hpp"
#include "powder/worker.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace powder;
void check(bool b,const char* reason){if(!b)throw std::runtime_error(reason);}
void near(double a,double b,double tol,const char* reason){check(std::abs(a-b)<=tol,reason);}
template<class F>void rejects(F f){bool yes=false;try{f();}catch(const std::exception&){yes=true;}check(yes,"invalid request rejected");}
bool same(const ParticleSystem& a,const ParticleSystem& b){if(a.particles.size()!=b.particles.size())return false;for(std::size_t i=0;i<a.particles.size();++i){auto& p=a.particles[i];auto& q=b.particles[i];if(p.id!=q.id||p.diameter_m!=q.diameter_m||p.position_m.x!=q.position_m.x||p.position_m.y!=q.position_m.y||p.position_m.z!=q.position_m.z)return false;}return true;}
void independent_check(const InitialBatch& b,const PowderSettings& s){
 double sum=0;for(std::size_t i=0;i<b.world.particles.size();++i){auto& p=b.world.particles[i];const double r=p.diameter_m/2;
  check(std::abs(p.position_m.x-s.centre_m.x)+r<=s.half_m.x+1e-14&&std::abs(p.position_m.y-s.centre_m.y)+r<=s.half_m.y+1e-14&&std::abs(p.position_m.z-s.centre_m.z)+r<=s.half_m.z+1e-14,"sphere fits spawn volume");
  check(box_contacts(p,b.world.boxes).empty(),"independent collision geometry finds no initial wall overlap");
  for(std::size_t j=0;j<i;++j){auto& q=b.world.particles[j];check(std::hypot(p.position_m.x-q.position_m.x,p.position_m.y-q.position_m.y,p.position_m.z-q.position_m.z)>=(p.diameter_m+q.diameter_m)/2,"no pair overlap");}
  sum+=3.14159265358979323846/6*p.diameter_m*p.diameter_m*p.diameter_m*s.density_kg_m3;
 }
 near(sum,b.report.mass_kg,1e-10,"independent sphere mass sum");
}
template<class F>auto wait(Worker& w,F predicate){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(15);while(std::chrono::steady_clock::now()<end){auto s=w.snapshot();if(predicate(*s))return s;std::this_thread::sleep_for(std::chrono::milliseconds(1));}throw std::runtime_error("worker timeout");}
int main(int argc,char** argv){try{
 check(argc==2,"geometry fixture path");ParticleSystem rig;std::ifstream f(argv[1]);rig.boxes=read_boxes(f);
 PowderSettings s;const auto text=serialize_settings(s);check(serialize_settings(parse_settings(text))==text,"canonical save/load roundtrip");auto a=make_initial(s,rig),b=make_initial(parse_settings(text),rig);check(a.report.count==300&&same(a.world,b.world),"300 mono bitwise reproducible after roundtrip");independent_check(a,s);near(a.report.mass_kg,25.132741228718345,1e-10,"300 independent reference mass");
 auto next=s;++next.seed;check(!same(a.world,make_initial(next,rig).world),"seed changes lattice sequence");
 s.distribution=SizeDistribution::UniformNumber;s.diameter_min_m=.03;s.diameter_max_m=.05;
 a=make_initial(s,rig);b=make_initial(parse_settings(serialize_settings(s)),rig);check(same(a.world,b.world),"distributed particles exact reproducibility");independent_check(a,s);double mean=0;int lower=0,upper=0;for(auto& p:a.world.particles){check(p.diameter_m>=.03&&p.diameter_m<.05,"uniform number diameter bounds");mean+=p.diameter_m/300;if(p.diameter_m<.04)++lower;else ++upper;}
 check(lower>0&&upper>0&&std::abs(mean-.04)<.002,"uniform sample uses both diameter intervals");std::cout<<"300 uniform-number: mass="<<a.report.mass_kg<<"kg, mean d="<<mean<<"m, candidates="<<a.report.candidates<<'\n';
 s.amount=AmountMode::Mass;s.count=0;s.target_mass_kg=10;s.mass_tolerance_kg=.1;a=make_initial(s,rig);check(std::abs(a.report.mass_error_kg)<=.1&&a.report.count>0,"mass-selected seeded prefix");independent_check(a,s);check(same(a.world,make_initial(parse_settings(serialize_settings(s)),rig).world),"mass mode roundtrip reproducibility");std::cout<<"Requested 10kg +/-0.1: count="<<a.report.count<<" actual="<<a.report.mass_kg<<" delta="<<a.report.mass_error_kg<<'\n';
 auto invalid=s;invalid.count=3;rejects([&]{validate_settings(invalid);});invalid=s;invalid.target_mass_kg=1e6;rejects([&]{make_initial(invalid,rig);});
 PowderSettings mono;mono.amount=AmountMode::Mass;mono.count=0;double unit=3.14159265358979323846/6*.04*.04*.04*2500;mono.target_mass_kg=10*unit;mono.mass_tolerance_kg=1e-12;auto ten=make_initial(mono,rig);check(ten.report.count==10,"ten complete particles requested by mass");
 mono.target_mass_kg=10.25*unit;mono.mass_tolerance_kg=unit*.01;rejects([&]{make_initial(mono,rig);});mono.mass_tolerance_kg=unit*.26;check(make_initial(mono,rig).report.count==10,"nearest prefix never changes particle mass");
 invalid=PowderSettings{};invalid.half_m={.001,.001,.001};rejects([&]{make_initial(invalid,rig);});invalid=PowderSettings{};invalid.max_candidates=1;rejects([&]{make_initial(invalid,rig);});
 invalid=PowderSettings{};invalid.centre_m=rig.boxes[2].centre_m;invalid.count=1;rejects([&]{make_initial(invalid,rig);});check(rig.particles.empty(),"failed initialization never modifies caller geometry");
 for(int i=0;i<8;++i){invalid=PowderSettings{};switch(i){case 0:invalid.density_kg_m3=-1;break;case 1:invalid.particle_material.poisson=.5;break;case 2:invalid.wall_material.young_pa=0;break;case 3:invalid.wall.linear.mu=-1;break;case 4:invalid.pair.overlap_cap=.1;break;case 5:invalid.dt_s=0;break;case 6:invalid.diameter_min_m=.06;break;case 7:invalid.spacing_factor=1;break;}rejects([&]{validate_settings(invalid);});}
 rejects([&]{parse_settings(text+"count 42\n");});rejects([&]{parse_settings(text.substr(0,text.size()/2));});auto bad=text;bad.replace(bad.find("POWDER_SETTINGS 2"),17,"POWDER_SETTINGS 9");rejects([&]{parse_settings(bad);});bad=text;bad.replace(bad.find("seed 42"),7,"seed -1");rejects([&]{parse_settings(bad);});
 // Wall parameters must affect wall forces but never particle-particle material mixing.
 PowderSettings h;h.law=ContactLaw::HertzMindlin;h.count=1;h.pair.cn=h.pair.ct=h.wall.cn=h.wall.ct=0;auto wh=make_initial(h,{}).world;check(wh.model.b.young_pa==h.particle_material.young_pa&&wh.wall_model->b.young_pa==h.wall_material.young_pa,"Hertz particle-particle versus particle-wall mapping");
 wh.gravity_m_s2={};wh.planes={{1,{0,0,1},0}};wh.particles[0].position_m={0,0,.0199};auto soft=wh,hard=wh;soft.wall_model->b.young_pa=1e5;hard.wall_model->b.young_pa=1e11;soft.step(1e-8);hard.step(1e-8);check(hard.particles[0].velocity_m_s.z>soft.particles[0].velocity_m_s.z,"wall modulus changes actual contact force");
 soft=wh;hard=wh;soft.planes.clear();hard.planes.clear();auto q=soft.particles[0];q.id=2;q.position_m.z+=.0399;soft.particles.push_back(q);hard.particles.push_back(q);soft.wall_model->b.young_pa=1e5;hard.wall_model->b.young_pa=1e11;soft.step(1e-8);hard.step(1e-8);check(same(soft,hard)&&soft.particles[0].velocity_m_s.z==hard.particles[0].velocity_m_s.z,"wall material cannot change pair force");
 auto linear=make_initial(PowderSettings{},{}).world;linear.particles.resize(1);linear.gravity_m_s2={};linear.particles[0].diameter_m=1;linear.particles[0].material.density_kg_m3=6/3.14159265358979323846;linear.particles[0].position_m={0,0,.49};linear.planes={{1,{0,0,1},0}};linear.model.linear={100,0,100,0,0};linear.wall_model->linear={400,0,100,0,0};linear.step(1e-5);near(linear.particles[0].velocity_m_s.z,.00004,1e-14,"wall linear stiffness used independently");
 auto cap=wh;cap.model.max_overlap_radius_ratio=1e-6;cap.step(1e-8);cap=wh;cap.wall_model->max_overlap_radius_ratio=1e-6;rejects([&]{cap.step(1e-8);});
 Worker worker;PowderSettings cfg;auto gen=worker.configure(cfg,rig);auto ready=wait(worker,[&](auto& r){return r.generation==gen&&r.ready;});check(ready->error.empty()&&ready->particles.size()==300&&ready->initial.configured,"background generation ready");check(ready->settings_text==serialize_settings(cfg),"snapshot records applied settings");auto held=ready;worker.toggle();invalid=cfg;invalid.max_candidates=1;gen=worker.configure(invalid,rig);auto failure=wait(worker,[&](auto& r){return r.generation==gen&&r.ready;});check(!failure->error.empty()&&failure->particles.empty()&&!failure->running,"impossible placement publishes no partial particles");gen=worker.configure(cfg,rig);ready=wait(worker,[&](auto& r){return r.generation==gen&&r.ready;});check(ready->error.empty()&&ready->steps==0&&held->particles.size()==300,"failed generation recoverable and old snapshot immutable");
 rejects([&]{make_initial(cfg,rig,[]{return true;});});
 std::cout<<"PASS settings, masses, seeded packing, wall material isolation, atomic failure, worker reset\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
