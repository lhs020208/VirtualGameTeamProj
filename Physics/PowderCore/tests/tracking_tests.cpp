#include "powder/worker.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
using namespace powder;
void check(bool v,const char* s){if(!v)throw std::runtime_error(s);}
void near(double a,double b,double eps,const char* s){check(std::abs(a-b)<=eps,s);}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::invalid_argument&){caught=true;}check(caught,"invalid input rejected");}
template<class F>auto wait(Worker& w,F f){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(std::chrono::steady_clock::now()<end){auto s=w.snapshot();if(f(*s))return s;std::this_thread::sleep_for(std::chrono::milliseconds(1));}throw std::runtime_error("worker timeout");}
Volume cube(double lo,double hi){return {{{{1,0,0},1},{{-1,0,0},1},{{0,1,0},1},{{0,-1,0},1},{{0,0,1},hi},{{0,0,-1},-lo}}};}
TrackingLayout simple(){TrackingLayout l;l.enabled=true;l.bottom=1;l.top=2;l.bore=cube(-100,100);l.bore.sides.resize(4);l.feed=cube(2,4);l.funnel=l.feed;l.collector=cube(-1,0);return l;}
Particle particle(ParticleId id,Vec3 v,double d=.1){Particle p;p.id=id;p.position_m=v;p.diameter_m=d;p.material.density_kg_m3=1000;return p;}
void conservation(const TrackingSummary& s,double mass){check(std::accumulate(s.counts.begin(),s.counts.end(),std::size_t(0))==s.total_count,"exclusive count conservation");near(std::accumulate(s.mass_kg.begin(),s.mass_kg.end(),0.0),mass,1e-10,"mass partition");near(s.mass_error_kg,0,1e-10,"mass balance");}
int main(int argc,char** argv){try{
 auto l=simple();Tracker t;std::vector<Particle> p{particle(9,{0,0,3})};t.reset(l,p);double mass=t.summary().total_mass_kg;
 p[0].position_m.z=0;t.advance(p,3);auto s=t.summary();check(s.passages==1&&s.unique_passed==1&&s.records[0].events.size()==2,"fast crossing of both planes");near(s.records[0].first_entry_s,1,1e-12,"entry interpolated physical time");near(s.records[0].first_exit_s,2,1e-12,"exit interpolated physical time");conservation(s,mass);
 p[0].position_m.z=3;t.advance(p,6);check(t.summary().crossings[2]==1&&t.summary().crossings[3]==1&&t.summary().passages==1,"reverse crossing is not forward passage");
 p[0].position_m.z=0;t.advance(p,9);check(t.summary().passages==2&&t.summary().unique_passed==1,"reentry unique versus repeated passages");
 t.reset(l,{particle(9,{0,0,3})});p[0].position_m.z=2;t.advance(p,1);p[0].position_m.z=1.5;t.advance(p,2);check(t.summary().crossings[0]==1,"endpoint no duplicate");p[0].position_m.z=3;t.advance(p,3);check(t.summary().returns==1&&t.summary().unique_returned==1,"admitted particle returned upstream");
 // A linear trajectory subdivided into steps has identical physical crossing times.
 Tracker coarse,fine;coarse.reset(l,{particle(1,{0,0,3})});fine.reset(l,{particle(1,{0,0,3})});coarse.advance({particle(1,{0,0,0})},3);
 for(int i=1;i<=30;++i)fine.advance({particle(1,{0,0,3-i*.1})},i*.1);
 check(coarse.summary().crossings==fine.summary().crossings,"partition-independent crossing counts");near(coarse.summary().records[0].first_exit_s,fine.summary().records[0].first_exit_s,1e-12,"partition-independent event time");
 // Side escape retires the admission; later entry via the side cannot fake a full traversal.
 t.reset(l,{particle(9,{0,0,3})});p[0].position_m={0,0,1.5};t.advance(p,1);p[0].position_m={2,0,1.5};t.advance(p,2);p[0].position_m={0,0,.5};t.advance(p,3);check(t.summary().passages==0,"side escape is not passage");
 t.reset(l,{particle(9,{0,0,1.5})});p[0].position_m={0,0,0};t.advance(p,1);check(t.summary().crossings[1]==1&&t.summary().passages==0,"initially inside is not admitted");
 t.reset(l,{particle(9,{2,0,3})});p[0].position_m={2,0,0};t.advance(p,1);check(t.summary().crossings[0]==0&&t.summary().crossings[1]==0,"outside aperture cannot pass");
 std::vector<Particle> mixed{particle(1,{0,0,3}),particle(2,{0,0,1.5},.2),particle(3,{0,0,-.5},.3),particle(4,{2,0,0},.4)};t.reset(l,mixed);mass=t.summary().total_mass_kg;for(auto n:t.summary().counts)check(n==1,"four disjoint regions");conservation(t.summary(),mass);
 std::reverse(mixed.begin(),mixed.end());t.advance(mixed,1);check(t.summary().records[0].id==1&&t.summary().records[0].location==Location::Feed,"ID tracking survives array reordering");
 rejects([&]{t.advance(mixed,1);});auto bad=mixed;bad[0].id=99;rejects([&]{t.advance(bad,2);});bad=mixed;bad[0].diameter_m*=2;rejects([&]{t.advance(bad,2);});bad.pop_back();rejects([&]{t.advance(bad,2);});conservation(t.summary(),mass);
 t.reset(l,mixed);check(t.summary().passages==0&&t.summary().records[0].events.empty(),"reset clears records");
 check(argc==3,"fixture args");std::ifstream boxes_file(argv[1]),groups_file(argv[2]);auto boxes=read_boxes(boxes_file);std::string magic;unsigned ver;groups_file>>magic>>ver;check(magic=="POWDER_TRACKING_GROUPS"&&ver==1,"group format");std::vector<FixedBox> groups[4];for(auto& g:groups){int n;groups_file>>n;for(int i=0;i<n;++i){std::uint64_t id;groups_file>>id;auto it=std::find_if(boxes.begin(),boxes.end(),[&](auto& b){return b.id==id;});check(it!=boxes.end(),"box ID mapping");g.push_back(*it);}}
 auto rig=tracking_layout(groups[0],groups[1],groups[2],groups[3]);near(rig.top,3.6,1e-10,"top from wall transform");near(rig.bottom,2.4,1e-10,"bottom from wall transform");
 std::vector<Particle> many;for(int i=0;i<300;++i)many.push_back(particle(i+1,{(i%5-2)*.05,((i/5)%5-2)*.05,4.5+(i/25)*.05},.04));t.reset(rig,many);check(t.summary().counts[0]==300,"actual rig 300 feed classification");conservation(t.summary(),t.summary().total_mass_kg);
 // Reproducible legacy undercount: two centres within bore, beyond sphere-clearance threshold.
 // Diagnostic kinematic paths only: does NOT reconstruct the old Chaos run.
 many.clear();for(int i=0;i<300;++i)many.push_back(particle(i+1,{i<298?0:.095,0,3.8},.06));t.reset(rig,many);int legacy=0;for(auto& q:many){bool fits=true;for(int k=0;k<16;++k){double a=2*3.141592653589793*k/16;if(q.position_m.x*std::cos(a)+q.position_m.y*std::sin(a)>.12-.03+.0015)fits=false;}if(fits)++legacy;q.position_m.z=2.2;}t.advance(many,1);check(legacy==298&&t.summary().unique_passed==300,"legacy 298 versus 300 centre crossings regression");conservation(t.summary(),t.summary().total_mass_kg);std::cout<<"Legacy predicate replay: 298; centre crossings: 300; historical cause remains unproven\n";
 // A wholly rigid rotation + translation carries the actual measurement planes with the rig.
 auto rotate=[](Vec3 v){return Vec3{v.z,v.y,-v.x};};auto transform=[&](Vec3 v){auto r=rotate(v);return Vec3{r.x+7,r.y-2,r.z+4};};
 for(auto& g:groups)for(auto& b:g){b.centre_m=transform(b.centre_m);for(auto& a:b.axes)a=rotate(a);}
 auto moved=tracking_layout(groups[0],groups[1],groups[2],groups[3]);t.reset(moved,{particle(1,transform({0,0,3.8}))});t.advance({particle(1,transform({0,0,2.2}))},1);check(t.summary().passages==1,"geometry-linked rotated/translated gates");near(t.summary().records[0].first_entry_s,.125,1e-10,"transformed entry time");
 groups[2][0].centre_m.x+=.1;rejects([&]{tracking_layout(groups[0],groups[1],groups[2],groups[3]);});
 // Worker tracks all fixed steps; a whole crossing may occur before the first rendered update.
 auto tiny=simple();tiny.top=.0001;tiny.bottom=-.0001;ParticleSystem w;w.gravity_m_s2={};w.model.law=ContactLaw::Linear;w.model.linear={1,0,1,0,0};auto q=particle(1,{0,0,.0005},1);q.velocity_m_s.z=-100;w.particles={q};Worker worker;auto gen=worker.reset(w,1e-5,tiny);wait(worker,[&](auto& x){return x.generation==gen&&x.ready;});worker.single_step();auto held=wait(worker,[](auto& x){return x.steps==1;});check(held->error.empty()&&held->tracking.passages==1,"worker per-step tracking");
 gen=worker.reset(w,1e-5,tiny);auto clean=wait(worker,[&](auto& x){return x.generation==gen&&x.ready;});check(clean->tracking.passages==0&&clean->tracking.records[0].events.empty()&&held->tracking.passages==1,"tracking snapshot immutable and reset isolated");
 std::cout<<"PASS tracking: conservation, timestamps, fast crossings, reverse/reentry, side escape, IDs, real rig transforms, worker/reset\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
