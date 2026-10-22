#include "powder/gpu.hpp"
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <array>
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
using namespace powder;using Clock=std::chrono::steady_clock;
double elapsed(Clock::time_point t){return std::chrono::duration<double>(Clock::now()-t).count();}
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(int argc,char** argv){try{check(argc==2,"usage: powder_gpu18 NEW_OUTPUT_DIR (fixed full target)");std::filesystem::path out(argv[1]);check(std::filesystem::create_directory(out),"output must be new");std::ofstream f(out/"windows.csv");f<<std::setprecision(17)<<"repeat,window,bodies,mass_a_kg,mass_b_kg,dt_s,time_s,compute_s,measurement_snapshot_s,solver_rate,inclusive_rate,contacts,wall_contacts,overlap_m,torque_z_nm,wall_work_j,momentum_residual,mix_index,device_buffers_bytes,private_bytes,peak_working_set_bytes,sort_gpu_s,contact_gpu_s,guard_gpu_s,commit_gpu_s\n";f.flush();
 const std::size_t n=1591550,side=117;const double dt=2e-8,pitch=.00199999,mass=sphere_properties(.002,{1500}).mass_kg;double minimum=INFINITY;bool accurate=true;double maxerror=0;unsigned completed=0;
 for(unsigned repeat=0;repeat<3;++repeat){auto start=Clock::now();GpuSpheres gpu(8100000000ull);std::vector<Particle> p;p.reserve(n);for(std::size_t i=0;i<n;++i){Particle a;a.id=i+1;a.diameter_m=.002;a.material.density_kg_m3=1500;a.position_m={double(i%side)*pitch,double((i/side)%side)*pitch,.01+double(i/(side*side))*pitch};p.push_back(a);}gpu.reset(p);MaterialContact model;model.law=ContactLaw::Linear;model.linear={10000,.01,2500,.0025,.4};gpu.enable_contacts(model,32,256);
 std::vector<GpuWall> walls;GpuWall floor;floor.normal={0,0,1};walls.push_back(floor);GpuWall paddle;paddle.kind=GpuWallKind::MovingBox;paddle.centre={-.00199999,.116,.125};paddle.half={.001,.12,.12};paddle.pivot=paddle.centre;paddle.omega={0,0,.1};walls.push_back(paddle);gpu.set_walls(walls);gpu.set_profiling(true);
 const double setup=elapsed(start);std::cout<<"repeat "<<repeat<<" setup "<<setup<<"s, full target allocated\n"<<std::flush;double previous_pz=0,previous_py=0,previous_px=0;
 for(unsigned window=0;window<6;++window){auto prior=gpu.timing();start=Clock::now();gpu.advance_contacts(dt,1);double compute=elapsed(start);start=Clock::now();auto r=gpu.contact_report();auto load=gpu.wall_report();auto state=gpu.snapshot();double px=0,py=0,pz=0;std::array<std::array<std::size_t,2>,8> cells{};
 for(std::size_t i=0;i<state.size();++i){const auto& a=state[i];check(a.id==i+1&&a.diameter_m==.002&&a.material.density_kg_m3==1500,"identity/material changed");px+=mass*a.velocity_m_s.x;py+=mass*a.velocity_m_s.y;pz+=mass*a.velocity_m_s.z;unsigned cell=(a.position_m.x>=.116?1u:0u)+(a.position_m.y>=.116?2u:0u)+(a.position_m.z>=.126?4u:0u);++cells[cell][i<n/2?0:1];}
 double error=std::max({std::abs(px-previous_px+load.reaction_force_n.x*dt),std::abs(py-previous_py+load.reaction_force_n.y*dt),std::abs(pz-previous_pz+n*mass*9.81*dt+load.reaction_force_n.z*dt)});previous_px=px;previous_py=py;previous_pz=pz;maxerror=std::max(maxerror,error);double variance=0;for(auto cell:cells){double count=double(cell[0]+cell[1]);if(count>0)variance+=count/double(n)*std::pow(double(cell[0])/count-.5,2);}double index=1-variance/.25;const double measurement=elapsed(start);const double rate=dt/(compute+measurement);if(window>0)minimum=std::min(minimum,rate);accurate=accurate&&error<1e-10&&r.max_overlap_m<1e-5&&r.directed_contacts>4000000;
 PROCESS_MEMORY_COUNTERS_EX mem{};check(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&mem),sizeof(mem))!=0,"memory measurement failed");f<<repeat<<','<<window<<','<<n<<','<<(n/2)*mass<<','<<(n/2)*mass<<','<<dt<<','<<gpu.physical_time_s()<<','<<compute<<','<<measurement<<','<<dt/compute<<','<<rate<<','<<r.directed_contacts/2<<','<<r.wall_contacts<<','<<r.max_overlap_m<<','<<load.reaction_torque_nm.z<<','<<load.work_to_particles_j<<','<<error<<','<<index<<','<<gpu_memory_plan(n,32).total_bytes<<','<<mem.PrivateUsage<<','<<mem.PeakWorkingSetSize<<','<<gpu.timing().sort_gpu_s-prior.sort_gpu_s<<','<<gpu.timing().contact_gpu_s-prior.contact_gpu_s<<','<<gpu.timing().guard_gpu_s-prior.guard_gpu_s<<','<<gpu.timing().commit_gpu_s-prior.commit_gpu_s<<'\n';f.flush();check(bool(f),"failed result write");++completed;std::cout<<"repeat "<<repeat<<" window "<<window<<" compute "<<compute<<"s rate "<<rate<<" contacts "<<r.directed_contacts/2<<'\n'<<std::flush;}
 }
 // A short-window necessary-condition gate cannot certify sustained or end-to-end performance.
 std::ofstream report(out/"acceptance.json");report<<std::setprecision(17)<<"{\"schema\":1,\"full_target_bodies\":"<<n<<",\"mass_kg\":"<<n*mass<<",\"q\":1,\"repeats\":3,\"completed_windows\":"<<completed<<",\"accuracy_gate\":"<<(accurate?"true":"false")<<",\"minimum_measured_rate\":"<<minimum<<",\"required_rate\":0.1,\"max_momentum_residual\":"<<maxerror<<",\"throughput_gate\":"<<(minimum>=.1?"true":"false")<<",\"end_to_end_completed\":false,\"accepted\":false,\"status\":\"NOT_ACCEPTED_FULL_PROCESS_REQUIRES_SUSTAINED_TEST\",\"residence_s\":null,\"render_included\":false,\"note\":\"Full mass dense moving-box admission test, not full process traversal. Do not claim mixing completion or 0.1x.\"}\n";check(bool(report),"acceptance write failed");return accurate?0:2;
 }catch(const std::exception& e){std::cerr<<"GPU18_REJECTED "<<e.what()<<'\n';return 1;}}
