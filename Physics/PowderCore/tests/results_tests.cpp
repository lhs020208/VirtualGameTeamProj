#include "powder/worker.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <cmath>
using namespace powder;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
void near(double a,double b){check(std::abs(a-b)<1e-12,"independent expected metric");}
template<class F>void rejects(F f){bool hit=false;try{f();}catch(const std::exception&){hit=true;}check(hit,"expected rejection");}
Volume cube(double lo,double hi){return {{{{1,0,0},1},{{-1,0,0},1},{{0,1,0},1},{{0,-1,0},1},{{0,0,1},hi},{{0,0,-1},-lo}}};}
TrackingLayout layout(){TrackingLayout l;l.enabled=true;l.top=2;l.bottom=1;l.bore=cube(-100,100);l.bore.sides.resize(4);l.feed=cube(2,4);l.funnel=l.feed;l.collector=cube(-1,0);return l;}
Particle particle(){Particle p;p.id=1;p.diameter_m=.1;p.position_m={0,0,3};p.velocity_m_s={0,0,-1};return p;}
template<class F>auto wait(Worker& w,F f){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(15);while(std::chrono::steady_clock::now()<end){auto s=w.snapshot();if(f(*s))return s;std::this_thread::sleep_for(std::chrono::milliseconds(1));}throw std::runtime_error("worker timeout");}
int main(int argc,char** argv){try{
 check(argc==2,"output fixture path");auto l=layout();auto p=particle();const double mass=3.14159265358979323846/6*.001*1000;
 SupplyReport q;q.configured=q.complete=true;q.planned=q.requested=q.inserted=1;q.planned_mass=q.requested_mass=q.inserted_mass=mass;
 Tracker tracker;tracker.reset(l,{p});ResultCriteria c{.5,.001,1,100};ResultMonitor m(c);m.observe({p},tracker.summary(),q,0);auto old=m.state().samples;
 p.position_m.z=0;tracker.advance({p},3);m.observe({p},tracker.summary(),q,3);near(m.state().current.gross_exit_kg,mass);near(m.state().current.net_exit_kg,mass);near(m.state().current.unique_pass_kg,mass);check(old->size()==1&&m.state().samples->size()==2,"published samples immutable");
 Snapshot s;s.ready=true;s.generation=7;s.steps=3;s.dt_s=1;s.simulation_seconds=3;s.particles={p};s.tracking=tracker.summary();s.supply=q;s.results=m.state();s.measurement=measurement_json(l);s.geometry=geometry_json({});PowderSettings settings;settings.count=1;settings.density_kg_m3=1000;settings.diameter_min_m=settings.diameter_max_m=.1;settings.centre_m={0,0,3};settings.velocity_m_s={0,0,-1};settings.gravity_m_s2={};settings.dt_s=1;settings.pair.linear={1e-6,0,1e-6,0,0};settings.wall.linear=settings.pair.linear;s.settings_text=serialize_settings(settings);s.initial.seed=settings.seed;
 auto files=export_results(s);check(files.size()==6,"complete pure export bundle");check(files.at("summary.json").find("\"residence_mean_s\":1")!=std::string::npos,"1 second analytically known transit");check(files.at("events.csv").find("1,1,2,")!=std::string::npos,"physical exit at t=2 retained between samples");
 std::filesystem::create_directories(argv[1]);for(auto& f:files)std::ofstream(std::filesystem::path(argv[1])/f.first,std::ios::binary)<<f.second;std::ofstream(std::filesystem::path(argv[1])/"complete.json")<<"{\"schema\":1,\"complete\":true}";std::ofstream(std::filesystem::path(argv[1])/"run.json")<<"{\"test_fixture\":true}";
 check(files==export_results(s),"same snapshot deterministic export");s.running=true;rejects([&]{export_results(s);});s.running=false;s.error="failure";rejects([&]{export_results(s);});s.error.clear();

 // A genuinely different density condition, same geometry and trajectory, doubles output mass.
 auto pb=particle();pb.material.density_kg_m3=2000;Tracker tb;tb.reset(l,{pb});auto qb=q;qb.planned_mass=qb.requested_mass=qb.inserted_mass=2*mass;ResultMonitor mb(c);mb.observe({pb},tb.summary(),qb,0);pb.position_m.z=0;tb.advance({pb},3);mb.observe({pb},tb.summary(),qb,3);auto sb=s;sb.particles={pb};sb.tracking=tb.summary();sb.supply=qb;sb.results=mb.state();settings.density_kg_m3=2000;sb.settings_text=serialize_settings(settings);
 const auto second_dir=std::filesystem::path(argv[1])/"condition_b";std::filesystem::create_directories(second_dir);for(auto& f:export_results(sb))std::ofstream(second_dir/f.first,std::ios::binary)<<f.second;std::ofstream(second_dir/"complete.json")<<"{\"schema\":1,\"complete\":true}";std::ofstream(second_dir/"run.json")<<"{\"test_fixture\":true}";
 // Reverse crossings change net discharge, repeated completed passage does not double unique mass.
 p.position_m.z=3;tracker.advance({p},6);m.observe({p},tracker.summary(),q,6);near(m.state().current.net_exit_kg,0);p.position_m.z=0;tracker.advance({p},9);m.observe({p},tracker.summary(),q,9);near(m.state().current.gross_exit_kg,2*mass);near(m.state().current.net_exit_kg,mass);near(m.state().current.unique_pass_kg,mass);
 // Distinguish temporary/long low speed, no discharge and completed supply independently.
 p=particle();p.velocity_m_s={};tracker.reset(l,{p});ResultMonitor slow(c);slow.observe({p},tracker.summary(),q,0);tracker.advance({p},.5);slow.observe({p},tracker.summary(),q,.5);near(slow.state().slow_seconds.at(1),.5);check(slow.state().current.flow_state=="temporary_no_outflow"&&slow.state().current.supply_state=="complete","supply completion not flow completion");tracker.advance({p},1);slow.observe({p},tracker.summary(),q,1);check(slow.state().current.long_slow_count==1&&slow.state().current.flow_state=="prolonged_no_outflow","threshold inclusive");p.velocity_m_s.x=1;tracker.advance({p},1.1);slow.observe({p},tracker.summary(),q,1.1);near(slow.state().slow_seconds.at(1),0);p.velocity_m_s={};tracker.advance({p},1.2);slow.observe({p},tracker.summary(),q,1.2);near(slow.state().slow_seconds.at(1),0);check(slow.state().current.slow_count==1,"slow onset has no fabricated prior duration");
 // New particles have age zero and no prior low-speed residence.
 Particle second=p;second.id=2;tracker.admit({second},1.2);tracker.advance({p,second},1.3);slow.observe({p,second},tracker.summary(),q,1.3);near(slow.state().slow_seconds.at(2),0);near(tracker.summary().records.back().birth_time_s,1.2);
 ResultCriteria bad=c;bad.sample_s=0;rejects([&]{ResultMonitor x(bad);});bad=c;bad.long_stall_s=-1;rejects([&]{ResultMonitor x(bad);});
 // Worker supplies final off-grid samples at export, resets histories, and pauses at capacity.
 Worker worker;PowderSettings cfg;cfg.count=1;ResultCriteria limit{cfg.dt_s,.001,1,2};auto gen=worker.configure(cfg,{}, {},limit);auto snap=wait(worker,[&](auto& x){return x.ready&&x.generation==gen;});worker.toggle();snap=wait(worker,[](auto& x){return x.results.sample_limit_reached;});check(snap->steps==2&&!snap->running&&snap->error.empty(),"capacity pauses safely at step two");check(export_results(*snap).at("metadata.json").find("\"sample_limit_reached\":true")!=std::string::npos,"capacity exported explicitly");worker.single_step();std::this_thread::sleep_for(std::chrono::milliseconds(5));check(worker.snapshot()->steps==2,"capacity cannot silently resume");
 gen=worker.configure(cfg,{});snap=wait(worker,[&](auto& x){return x.ready&&x.generation==gen;});check(snap->results.samples->size()==1&&!snap->results.sample_limit_reached,"reset clears monitoring");worker.single_step();snap=wait(worker,[](auto& x){return x.steps==1;});auto end=export_results(*snap);check(snap->results.samples->size()==1,"off-grid point not scheduled");std::istringstream lines(end.at("timeseries.csv"));std::string line,last;int rows=0;while(std::getline(lines,line)){last=line;++rows;}check(rows==3,"header, initial and final off-grid row");near(std::stod(last.substr(0,last.find(','))),cfg.dt_s);check(end.at("summary.json").find("\"residence_mean_s\":null")!=std::string::npos,"no completion is null, not zero");
 std::cout<<"Result metrics, residence, snapshots, limits and export passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
