#include "powder/coupled.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <chrono>
int main(int argc,char** argv){try{
 if(argc!=5)throw std::runtime_error("usage: output count duration_s pressure_drop_pa");
 auto out=std::filesystem::path(argv[1]);if(std::filesystem::exists(out))throw std::runtime_error("output exists");std::filesystem::create_directories(out);
 auto w=powder::coupled_demo(unsigned(std::stoul(argv[2])),std::stod(argv[4]));double duration=std::stod(argv[3]),dt=5.e-6;
 if(!std::isfinite(duration)||duration<=0||duration>1)throw std::runtime_error("duration must be 0..1s");
 std::ofstream trace(out/"trace.csv");trace<<"t,count,gas_balance_kg,particle_balance_kg,exchange_Ns,max_overlap_m,min_epsilon,max_particle_speed\n"<<std::setprecision(17);
 const auto start=std::chrono::steady_clock::now();std::uint64_t steps=static_cast<std::uint64_t>(std::ceil(duration/dt));powder::CoupledReport r;
 for(std::uint64_t i=0;i<steps;++i){r=w.step(dt);if(i%1000==0||i+1==steps)trace<<r.time<<','<<r.count<<','<<r.gas_balance_error<<','<<r.particle_balance_error<<','<<r.exchange_error<<','<<r.max_overlap<<','<<r.min_epsilon<<','<<r.max_speed<<'\n';}
 double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 std::ofstream result(out/"result.json");result<<std::setprecision(17)<<"{\"simulated_s\":"<<r.time<<",\"wall_s\":"<<wall<<",\"rate\":"<<r.time/wall<<",\"count\":"<<r.count<<",\"gas_balance_kg\":"<<r.gas_balance_error<<",\"particle_balance_kg\":"<<r.particle_balance_error<<",\"exchange_error_Ns\":"<<r.exchange_error<<",\"validated_real_material\":false}";
 std::ofstream profile(out/"profile.json");profile<<std::setprecision(17)<<"{\"gas_s\":"<<w.timing.gas_s<<",\"coupling_s\":"<<w.timing.coupling_s<<",\"dem_s\":"<<w.timing.dem_s<<",\"commit_s\":"<<w.timing.commit_s<<"}";
 std::ofstream particles(out/"particles.csv");particles<<"id,type,x,y,z,radius_m\n";for(auto& b:w.dem.bodies)for(auto& p:powder::clump_particles(b))particles<<b.id<<','<<b.type<<','<<p.position_m.x<<','<<p.position_m.y<<','<<p.position_m.z<<','<<p.diameter_m/2<<'\n';
 if(!trace||!result||!particles)throw std::runtime_error("output write failed");std::cout<<"PASS coupled rate="<<r.time/wall<<" gas_error="<<r.gas_balance_error<<'\n';
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
