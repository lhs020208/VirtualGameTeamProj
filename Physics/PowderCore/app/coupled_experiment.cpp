#include "powder/coupled.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cmath>
using namespace powder;
int main(int argc,char** argv){try{
 if(argc<3||argc>5)throw std::runtime_error("output scenario [duration] [dt]");int mode=std::stoi(argv[2]);double duration=argc>3?std::stod(argv[3]):(mode==2||mode==5?.3:.12),dt=argc>4?std::stod(argv[4]):1e-7;
 if(!std::isfinite(duration)||duration<=0||duration>.5||!std::isfinite(dt)||dt<=0||dt>2e-7)throw std::runtime_error("invalid time range");
 std::filesystem::path out(argv[1]);if(std::filesystem::exists(out))throw std::runtime_error("output exists");std::filesystem::create_directories(out);auto w=coupled_experiment(mode);
 std::ofstream trace(out/"trace.csv");trace<<std::setprecision(17)<<"time,count,particle_kg,injected_kg,exited_kg,filter_kg,gas_error_kg,particle_error_kg,max_overlap_m,max_speed\n";
 auto begin=std::chrono::steady_clock::now();double maxOverlap=0,maxGasError=0,maxParticleError=0,maxExchange=0,maxFilter=0,maxZ=0;std::size_t steps=std::size_t(std::ceil(duration/dt));
 for(std::size_t i=0;i<steps;++i){auto r=step_coupled_experiment(w,mode,dt);maxOverlap=std::max(maxOverlap,r.max_overlap);maxGasError=std::max(maxGasError,std::abs(r.gas_balance_error));maxParticleError=std::max(maxParticleError,std::abs(r.particle_balance_error));maxExchange=std::max(maxExchange,r.exchange_error);
  if(mode==2||mode==5)maxFilter=std::max(maxFilter,coupled_filter_mass(w));for(const auto& b:w.dem.bodies)maxZ=std::max(maxZ,b.position_m.z);
  if(i%5000==0||i+1==steps)trace<<r.time<<','<<r.count<<','<<r.particle_mass<<','<<w.injected_mass<<','<<w.exited_mass<<','<<((mode==2||mode==5)?coupled_filter_mass(w):0)<<','<<r.gas_balance_error<<','<<r.particle_balance_error<<','<<r.max_overlap<<','<<r.max_speed<<'\n';
 }
 auto r=w.report();double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();bool complete=duration>=(mode==2||mode==5?.299:.119);
 bool passed=maxGasError<1e-12&&maxParticleError<1e-15&&maxExchange<1e-15&&maxOverlap<.00005;
 if(complete){if(mode==1||mode==6)passed=passed&&w.injected_mass>r.particle_mass&&w.used_ids.size()==6&&w.exited_mass>0;
 if(mode==3)passed=passed&&w.exited_by_face[1]==0&&w.used_ids.size()==6;
 if(mode==4)passed=passed&&w.exited_mass==0&&r.count==6;
 if(mode==2)passed=passed&&r.count==3&&w.exited_mass==0&&coupled_filter_mass(w)>0&&coupled_filter_mass(w)<=r.particle_mass&&maxZ>.008;
 if(mode==5)passed=passed&&r.count==3&&w.exited_mass==0&&coupled_filter_mass(w)==0&&maxFilter>0;}
 std::ofstream j(out/"result.json");j<<std::setprecision(17)<<"{\"scenario\":"<<mode<<",\"completed_protocol\":"<<(complete?"true":"false")<<",\"passed\":"<<(passed?"true":"false")<<",\"time_s\":"<<r.time<<",\"wall_s\":"<<wall<<",\"rate\":"<<r.time/wall<<",\"count\":"<<r.count<<",\"supplied_kg\":"<<w.injected_mass<<",\"retained_kg\":"<<r.particle_mass<<",\"exited_kg\":"<<w.exited_mass<<",\"inlet_backflow_kg\":"<<w.exited_by_face[0]<<",\"outlet_kg\":"<<w.exited_by_face[1]<<",\"filter_kg\":"<<((mode==2||mode==5)?coupled_filter_mass(w):0)<<",\"max_filter_kg\":"<<maxFilter<<",\"max_overlap_m\":"<<maxOverlap<<",\"max_gas_error_kg\":"<<maxGasError<<",\"max_particle_error_kg\":"<<maxParticleError<<",\"max_exchange_Ns\":"<<maxExchange<<",\"minimum_accepted_dt_s\":"<<w.minimum_step_s<<",\"calibrated\":false}";
 std::ofstream p(out/"particles.csv");p<<std::setprecision(17)<<"id,type,x,y,z,radius\n";for(const auto& b:w.dem.bodies)for(const auto& a:clump_particles(b))p<<b.id<<','<<b.type<<','<<a.position_m.x<<','<<a.position_m.y<<','<<a.position_m.z<<','<<a.diameter_m/2<<'\n';
 if(!j||!p||!trace)throw std::runtime_error("write failed");std::cout<<"scenario="<<mode<<" passed="<<passed<<" count="<<r.count<<" exited="<<w.exited_mass<<" filter="<<coupled_filter_mass(w)<<" rate="<<r.time/wall<<'\n';return passed?0:2;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
