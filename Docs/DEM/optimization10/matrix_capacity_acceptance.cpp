#include "powder/clump.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <numeric>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif
using namespace powder;
using Clock=std::chrono::steady_clock;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"missing capacity rejection");}
double elapsed(Clock::time_point t){return std::chrono::duration<double>(Clock::now()-t).count();}
ClumpWorld fixture(std::size_t n,bool dense,unsigned threads){
 ClumpWorld w;w.maximum_bodies=n;w.maximum_pebbles=n;w.maximum_candidates=4000000;w.cpu_threads=threads;w.parallel_min_jobs=128;
 MaterialContact m;m.law=ContactLaw::Linear;m.linear={10000,.01,2500,.0025,.4};w.pair_models={{{0,0},m},{{0,1},m},{{1,1},m}};w.wall_models={{0,m},{1,m}};
 const auto side=static_cast<std::size_t>(std::ceil(std::cbrt(double(n))));const double pitch=dense?.00199999:.004;
 w.bodies.reserve(n);
 for(std::size_t i=0;i<n;++i){auto b=make_clump(i+1,static_cast<unsigned>(i%2),{{{},.002}},1500);b.position_m={double(i%side)*pitch,double((i/side)%side)*pitch,.01+double(i/(side*side))*pitch};w.bodies.push_back(std::move(b));}
 return w;
}
std::uint64_t digest(const ClumpWorld& w){std::uint64_t h=14695981039346656037ull;for(unsigned char c:serialize_clump_state(w)){h^=c;h*=1099511628211ull;}return h;}
void selftest(){
 auto w=fixture(1000,false,1);w.maximum_bodies=300;auto before=digest(w);rejects([&]{w.step(2e-8);});check(digest(w)==before&&w.physical_time_s==0,"body cap atomicity");
 w.maximum_bodies=1000;w.maximum_pebbles=999;rejects([&]{w.step(2e-8);});check(digest(w)==before,"pebble cap atomicity");w.maximum_pebbles=1000;w.step(2e-8);
 auto d=fixture(1000,true,8);d.maximum_candidates=10;before=digest(d);rejects([&]{d.step(2e-8);});check(digest(d)==before&&d.histories()==0,"candidate cap atomicity");d.maximum_candidates=4000000;d.step(2e-8);auto ref=d;d.maximum_candidates=1;rejects([&]{d.step(2e-8);});check(digest(d)==digest(ref)&&d.histories()==ref.histories(),"cached cap atomicity");d.maximum_candidates=4000000;d.step(2e-8);ref.step(2e-8);check(digest(d)==digest(ref),"failure recovery history");
 for(bool dense:{false,true}){auto a=fixture(128,dense,1),b=a,c=a;b.cpu_threads=8;b.parallel_min_jobs=1;c.use_spatial_search=false;for(int i=0;i<30;++i){a.step(2e-8);b.step(2e-8);c.step(2e-8);check(serialize_clump_state(a)==serialize_clump_state(b)&&serialize_clump_state(a)==serialize_clump_state(c),"large-contract reference parity");}}
 // Explicit exhaustive fallback remains bounded, even for extreme coordinates.
 std::vector<Particle> nodes(20);std::vector<std::size_t> owners(20);for(std::size_t i=0;i<20;++i){nodes[i].diameter_m=.002;nodes[i].position_m.x=1e20;owners[i]=i;}
 rejects([&]{clump_candidates(nodes,owners,true,10);});rejects([&]{clump_candidates(nodes,owners,false,10);});
 std::cout<<"PASS default/explicit capacities, preallocation rejection, grid/exhaustive/cache budgets, failure recovery, serial/parallel/exhaustive exact parity\n";
}
int main(int argc,char** argv){try{if(argc==1){selftest();return 0;}check(argc==5,"usage: powder_capacity10 count threads dense steps");
 auto n=std::stoull(argv[1]);auto threads=static_cast<unsigned>(std::stoul(argv[2]));bool dense=std::stoi(argv[3])!=0;int steps=std::stoi(argv[4]);check(n>=2&&n<=2000000&&steps>=5&&steps<=10000,"benchmark bounds");
 const double dt=2e-8;auto begin=Clock::now();auto w=fixture(n,dense,threads);double setup=elapsed(begin);const double mass_one=sphere_properties(.002,{1500}).mass_kg;
 const auto side=static_cast<std::size_t>(std::ceil(std::cbrt(double(n))));const double pitch=dense?.00199999:.004;
 ClumpProfile p;ClumpReport report;begin=Clock::now();report=w.step_profiled(dt,&p);double cold=elapsed(begin);const auto cold_profile=p;p={};
 std::vector<double> windows;double overlap=report.max_overlap_m;std::size_t contacts=report.pair_contacts;
 begin=Clock::now();for(int i=0;i<steps;++i){auto t=Clock::now();report=w.step_profiled(dt,&p);windows.push_back(elapsed(t));overlap=std::max(overlap,report.max_overlap_m);contacts=std::max(contacts,report.pair_contacts);}double seconds=elapsed(begin);
 double position_error=0,momentum_error=0;Vec3 momentum{};double energy=0;
 for(std::size_t i=0;i<w.bodies.size();++i){const auto& b=w.bodies[i];momentum.x+=mass_one*b.velocity_m_s.x;momentum.y+=mass_one*b.velocity_m_s.y;momentum.z+=mass_one*b.velocity_m_s.z;energy+=clump_kinetic_energy(b);
  if(!dense){double expected=.01+double(i/(side*side))*pitch-9.81*dt*dt*(steps+1.)*(steps+2.)/2;position_error=std::max(position_error,std::abs(b.position_m.z-expected));}}
 momentum_error=std::hypot(momentum.x,momentum.y,momentum.z+n*mass_one*9.81*w.physical_time_s);
 check(position_error<1e-12&&momentum_error<1e-12+1e-9*n*mass_one*9.81*w.physical_time_s&&overlap/.001<.01,"capacity accuracy gate");
 check(std::abs(report.mass_kg-n*mass_one)<1e-10+1e-10*n*mass_one,"mass residual");auto hash=digest(w);
 std::size_t working=0,peak=0,private_bytes=0;
#ifdef _WIN32
 PROCESS_MEMORY_COUNTERS_EX memory{};if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory))){working=memory.WorkingSetSize;peak=memory.PeakWorkingSetSize;private_bytes=memory.PrivateUsage;}
#endif
 std::cout<<std::setprecision(17)<<"{\"passed\":true,\"count\":"<<n<<",\"threads\":"<<threads<<",\"dense\":"<<(dense?"true":"false")<<",\"steps\":"<<steps<<",\"radius_m\":0.001,\"density\":1500,\"dt_s\":"<<dt<<",\"mass_kg\":"<<report.mass_kg<<",\"mass_a_kg\":"<<((n+1)/2)*mass_one<<",\"mass_b_kg\":"<<(n/2)*mass_one<<",\"physical_s\":"<<w.physical_time_s<<",\"setup_s\":"<<setup<<",\"cold_step_s\":"<<cold<<",\"loop_s\":"<<seconds<<",\"rate\":"<<steps*dt/seconds<<",\"minimum_step_rate\":"<<dt/(*std::max_element(windows.begin(),windows.end()))<<",\"cold_inclusive_rate\":"<<(steps+1)*dt/(cold+seconds)<<",\"freefall_error_m\":"<<position_error<<",\"momentum_error\":"<<momentum_error<<",\"max_overlap_radius_ratio\":"<<overlap/.001<<",\"max_pair_contacts\":"<<contacts<<",\"history_count\":"<<w.histories()<<",\"kinetic_j\":"<<energy<<",\"state_fnv64\":\""<<hash<<"\",\"workspace_bytes\":"<<w.workspace_capacity_bytes()<<",\"neighbor_bytes\":"<<p.neighbor_capacity_bytes<<",\"working_set_bytes\":"<<working<<",\"peak_working_set_bytes\":"<<peak<<",\"private_bytes\":"<<private_bytes<<",\"phase_s\":[";
 for(std::size_t i=0;i<p.seconds.size();++i)std::cout<<(i?",":"")<<p.seconds[i];std::cout<<"],\"cold_phase_s\":[";for(std::size_t i=0;i<cold_profile.seconds.size();++i)std::cout<<(i?",":"")<<cold_profile.seconds[i];std::cout<<"]}\n";return 0;
 }catch(const std::exception& e){std::cerr<<"CAPACITY_REJECTED "<<e.what()<<'\n';return 2;}}
