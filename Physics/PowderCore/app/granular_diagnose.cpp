// Diagnostic replay of the current 50k virtual binary fixture, not a general preset loader.
#include "powder/granular.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <thread>
using Clock=std::chrono::steady_clock;
int main(int argc,char** argv){try{
 if(argc!=9)throw std::runtime_error("usage: initial.bin output steps batch local profile(0/1/2) readback_ms rate(0=uncapped)");
 const auto input=std::filesystem::path(argv[1]),out=std::filesystem::path(argv[2]);
 const unsigned steps=std::stoul(argv[3]),batch=std::stoul(argv[4]),local=std::stoul(argv[5]);const int profile=std::stoi(argv[6]);const double readms=std::stod(argv[7]),rate=std::stod(argv[8]);
 if(!steps||batch<1||batch>128||profile<0||profile>2||readms<0||rate<0)throw std::runtime_error("invalid diagnostic options");
 if(std::filesystem::exists(out))throw std::runtime_error("output already exists");std::filesystem::create_directories(out);
 powder::GranularConfig c;c.count=50000;c.dt=.000375f;c.iterations=16;c.test_mixer=true;c.binary_materials=true;c.rotation=true;c.rolling_friction=.02f;c.cross_friction=.35f;c.candidates=true;c.candidate_reuse=true;c.kernel_optimize=true;c.profile=profile>0;c.profile_split=profile==2;c.diagnostic_local_size=local;
 c.materials[0].radius_m=.0063647064;c.materials[0].density_kg_m3=900;c.materials[0].particle_friction=.3;c.materials[0].wall_friction=.25;
 c.materials[1].radius_m=.005;c.materials[1].density_kg_m3=1500;c.materials[1].particle_friction=.5;c.materials[1].wall_friction=.45;
 if(std::filesystem::file_size(input)!=c.count*sizeof(powder::Grain))throw std::runtime_error("fixture must contain exactly 50000 float4 records");
 std::vector<powder::Grain> initial(c.count),velocity(c.count);std::ifstream f(input,std::ios::binary);f.read(reinterpret_cast<char*>(initial.data()),initial.size()*sizeof(powder::Grain));if(!f)throw std::runtime_error("initial read failed");
 powder::GranularGPU solver(c,initial,velocity);auto start=Clock::now(),lastRead=start;unsigned done=0,reads=0;
 // Match fixed batch32 UE control: first rotating batch starts at step2672.
 // Preserve this boundary across batch comparisons; never alter dt/iterations.
 while(done<steps){unsigned n=std::min(batch,steps-done);if(done<2672)n=std::min(n,2672-done);
  if(rate>0)std::this_thread::sleep_until(start+std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>((done+n)*double(c.dt)/rate)));
  solver.advance(n,done<2672?0.f:30.f,false);done+=n;auto now=Clock::now();if(readms>0&&std::chrono::duration<double,std::milli>(now-lastRead).count()>=readms){solver.snapshot();lastRead=Clock::now();++reads;}
 }
 double wall=std::chrono::duration<double>(Clock::now()-start).count();auto state=solver.snapshot();std::ofstream bin(out/"final.bin",std::ios::binary);bin.write(reinterpret_cast<const char*>(state.data()),state.size()*sizeof(powder::Grain));
 std::ofstream timing(out/"timing.json");powder::write_granular_timing(timing,solver.timing());
 std::ofstream result(out/"result.json");result<<std::setprecision(17)<<"{\"wall_s\":"<<wall<<",\"time_s\":"<<solver.time()<<",\"rate\":"<<solver.time()/wall<<",\"angle_rad\":"<<solver.angle()<<",\"steps\":"<<steps<<",\"batch\":"<<batch<<",\"local\":"<<local<<",\"profile\":"<<profile<<",\"readbacks\":"<<reads<<",\"target_rate\":"<<rate<<",\"error\":\"\"}";
 if(!bin||!timing||!result)throw std::runtime_error("output write failed");std::cout<<"rate="<<solver.time()/wall<<" device="<<solver.device()<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
