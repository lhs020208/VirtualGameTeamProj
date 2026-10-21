#include "powder/clump.hpp"
#include "powder/clump_worker.hpp"
#include <atomic>
#include <future>
#include <iostream>
#include <iomanip>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif
using namespace powder;
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"expected rejection");}
ClumpWorld fixture(int n,bool clumps=false,bool hertz=false,bool walls=false){
 ClumpWorld w;w.gravity_m_s2={};MaterialContact m;m.law=hertz?ContactLaw::HertzMindlin:ContactLaw::Linear;
 m.linear={10000,8,2500,2,.4};m.rolling={1000,1,.1};m.adhesion_gamma_j_m2=.001;
 w.pair_models={{{0,0},m},{{0,1},m},{{1,1},m}};w.wall_models={{0,m},{1,m}};
 for(int i=0;i<n;++i){auto b=make_clump(i+1,i%2,clumps?std::vector<Pebble>{{{0,-.02,0},.04},{{0,.02,0},.04}}:std::vector<Pebble>{{{},.04}},2500);
  b.position_m={(i%8)*.039999,(i/8)*(clumps?.080001:.039999),.019999};
  b.velocity_m_s={.001*(i%2?1:-1),.001,0};b.angular_momentum_world={1e-8,2e-8,3e-8};w.bodies.push_back(b);
 }
 if(walls){FixedBox floor;floor.id=1;floor.centre_m={0,0,-.05};floor.half_m={4,4,.05};
  w.meshes.emplace_back(triangulated_box(floor));PrescribedMotion motion;motion.rpm=0;motion.linear_velocity_m_s={.001,0,0};
  // Moving wall on the opposite side, both static and moving histories exercised.
  floor.centre_m.z=.089998;w.moving_meshes.emplace_back(StaticTriangleMesh(triangulated_box(floor)),motion);
 }
 return w;
}
void compare(const ClumpWorld& a,const ClumpWorld& b){check(serialize_clump_state(a)==serialize_clump_state(b),"body bit parity");check(a.histories()==b.histories(),"history count parity");check(a.physical_time_s==b.physical_time_s&&a.moving_wall_work_j==b.moving_wall_work_j,"clock/work parity");}
void tests(){
 ParallelExecutor executor;std::vector<int> slots(257);
 for(unsigned n:{2u,4u,8u}){executor.run(slots.size(),n,1,[&](std::size_t i){++slots[i];});}
 for(auto x:slots)check(x==3,"missing/duplicate jobs");
 for(int repeat=0;repeat<30;++repeat){try{executor.run(257,8,1,[](std::size_t i){if(i==2||i==150)throw std::runtime_error(std::to_string(i));});check(false,"missing error");}catch(const std::runtime_error& e){check(std::string(e.what())=="2","unstable first error");}}
 executor.run(257,4,1,[&](std::size_t i){slots[i]=0;});for(auto x:slots)check(x==0,"failed barrier recovery");
 for(bool clumps:{false,true})for(bool hertz:{false,true})for(bool walls:{false,true})for(unsigned threads:{2u,4u,8u}){
  auto a=fixture(32,clumps,hertz,walls),b=a;b.cpu_threads=threads;b.parallel_min_jobs=1;ClumpProfile ap,bp;
  for(int step=0;step<200;++step){auto ar=a.step_profiled(1e-8,&ap),br=b.step_profiled(1e-8,&bp);compare(a,b);
   check(ar.pair_contacts==br.pair_contacts&&ar.wall_contacts==br.wall_contacts&&ar.max_overlap_m==br.max_overlap_m,"report parity");
   check(br.pair_lanes==threads&&br.integration_lanes==threads,"parallel path not exercised");
   for(std::size_t j=0;j<ar.moving_wall_loads.size();++j){const auto& x=ar.moving_wall_loads[j];const auto& y=br.moving_wall_loads[j];check(x.reaction_force_n.z==y.reaction_force_n.z&&x.reaction_torque_nm.z==y.reaction_torque_nm.z&&x.power_to_particles_w==y.power_to_particles_w,"reaction parity");}
  }
  check(ap.pair_distance_checks==bp.pair_distance_checks&&ap.sphere_integrations==bp.sphere_integrations,"profile counter parity");
  auto copy=b;check(copy.resident_cpu_workers()==0,"copied executor shared workers");
  auto future=std::async(std::launch::async,[copy]()mutable{for(int i=0;i<50;++i)copy.step(1e-8);return copy;});
  for(int i=0;i<50;++i)b.step(1e-8);compare(b,future.get());
  auto prior=b;rejects([&]{b.step(1);});compare(b,prior);b.step(1e-8);prior.step(1e-8);compare(b,prior);
  // Reconfiguration and history expiry must preserve continuation.
  a=b;a.cpu_threads=1;b.bodies.pop_back();a.bodies.pop_back();b.pair_models[{0,1}].mu=.25;a.pair_models=b.pair_models;
  for(int i=0;i<10;++i){a.step(1e-8);b.step(1e-8);}compare(a,b);
 }
 auto a=fixture(32),b=a;b.cpu_threads=8;b.parallel_min_jobs=100000;auto r=b.step(1e-8);a.step(1e-8);compare(a,b);check(r.pair_lanes==1&&r.integration_lanes==1&&b.resident_cpu_workers()==0,"small work fallback");
 // Exceptions inside parallel integration and force evaluation do not commit state.
 for(bool pair:{false,true}){auto w=fixture(32);w.cpu_threads=4;w.parallel_min_jobs=1;if(pair)w.bodies[1].position_m=w.bodies[0].position_m;else w.bodies[0].angular_momentum_world.z=1e9;
  auto prior=w;rejects([&]{w.step(1e-8);});compare(w,prior);
 }
 for(unsigned invalid:{0u,33u}){b.cpu_threads=invalid;rejects([&]{b.step(1e-8);});}
 // Momentum conservation in an isolated closed contact network.
 auto w=fixture(32);w.cpu_threads=4;w.parallel_min_jobs=1;auto momentum=[](const ClumpWorld& world){Vec3 p{};for(const auto& body:world.bodies){double mass=clump_properties(body.pebbles,body.density_kg_m3).mass_kg;p.x+=mass*body.velocity_m_s.x;p.y+=mass*body.velocity_m_s.y;p.z+=mass*body.velocity_m_s.z;}return p;};auto before=momentum(w);for(int i=0;i<1000;++i)w.step(1e-8);auto after=momentum(w);check(std::hypot(before.x-after.x,before.y-after.y,before.z-after.z)<1e-12,"momentum drift");
 ClumpRunState state;state.World=fixture(32);state.World.cpu_threads=4;state.World.parallel_min_jobs=1;state.Dt=1e-8;ClumpWorker worker(state);auto expected=state.World;
 for(int i=0;i<10;++i){worker.single_step();expected.step(state.Dt);}auto cp=worker.checkpoint();compare(expected,cp.World);worker.pause();
 std::cout<<"PASS 24 contact-law/clump/wall/thread combinations; exact continuation, concurrent copies, failure barriers, momentum, worker checkpoint\n";
}
int main(int argc,char** argv){try{if(argc==1){tests();return 0;}
 check(argc==5,"usage: parallel_tests count threads clumps steps");int count=std::stoi(argv[1]),threads=std::stoi(argv[2]),clumps=std::stoi(argv[3]),steps=std::stoi(argv[4]);check(count>=1&&count<=300&&steps>0,"benchmark arguments");
 auto w=fixture(count,clumps==1,false,clumps==2);w.cpu_threads=static_cast<unsigned>(threads);w.parallel_min_jobs=1;
 if(clumps==3){for(auto& body:w.bodies){body.position_m.x*=3;body.position_m.y*=3;body.position_m.z=1;}w.gravity_m_s2={0,0,-9.81};}
 for(int i=0;i<100;++i)w.step(1e-8);ClumpProfile p;ClumpReport report;auto start=std::chrono::steady_clock::now();
 for(int i=0;i<steps;++i)report=w.step_profiled(1e-8,&p);
 double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();std::size_t working=0,private_bytes=0;
#ifdef _WIN32
 PROCESS_MEMORY_COUNTERS_EX memory{};if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory))){working=memory.WorkingSetSize;private_bytes=memory.PrivateUsage;}
#endif
 std::cout<<std::setprecision(17)<<"{\"count\":"<<count<<",\"threads\":"<<threads<<",\"clumps\":"<<clumps<<",\"steps\":"<<steps<<",\"wall_s\":"<<seconds<<",\"sim_s_per_wall_s\":"<<steps*1e-8/seconds<<",\"workspace_bytes\":"<<w.workspace_capacity_bytes()<<",\"working_set_bytes\":"<<working<<",\"private_bytes\":"<<private_bytes<<",\"resident_workers\":"<<w.resident_cpu_workers()<<",\"pair_lanes\":"<<report.pair_lanes<<",\"integration_lanes\":"<<report.integration_lanes<<",\"pair_s\":"<<p.seconds[1]<<",\"integration_s\":"<<p.seconds[6]<<"}\n";
 return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

