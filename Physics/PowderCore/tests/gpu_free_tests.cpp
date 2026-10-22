#include "powder/gpu.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
using namespace powder;
void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
int main(int argc,char** argv){try{
 GpuSpheres gpu;const auto device=gpu.device();check(device.fp64_config!=0,"FP64 required");std::vector<Particle> reference;for(unsigned i=0;i<257;++i){Particle p;p.id=i+1;p.diameter_m=.002+(i%3)*.0001;p.material.density_kg_m3=1500;p.position_m={i*.003,0,.5};p.velocity_m_s={.01,0,0};reference.push_back(p);}gpu.reset(reference);
 const double dt=1e-5;const Vec3 gravity{0,0,-9.81},force{.00001,0,0};gpu.advance_free(dt,1000,gravity,force);
 for(unsigned step=0;step<1000;++step)for(auto& p:reference){ForceAccumulator f;f.add(force);f.add_gravity(sphere_properties(p.diameter_m,p.material).mass_kg,gravity);integrate_translation(p,f,dt);}
 const auto actual=gpu.snapshot();double error=0;for(std::size_t i=0;i<actual.size();++i){error=std::max(error,std::abs(actual[i].position_m.x-reference[i].position_m.x));error=std::max(error,std::abs(actual[i].position_m.z-reference[i].position_m.z));error=std::max(error,std::abs(actual[i].velocity_m_s.x-reference[i].velocity_m_s.x));error=std::max(error,std::abs(actual[i].velocity_m_s.z-reference[i].velocity_m_s.z));}check(error<1e-11,"GPU/CPU free integration mismatch");check(std::abs(gpu.physical_time_s()-.01)<1e-15,"GPU clock");
 bool rejected=false;try{gpu.advance_free(-1,1);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"invalid time not rejected");auto invalid=reference;invalid[1].id=invalid[0].id;rejected=false;try{gpu.reset(invalid);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"duplicate ID not rejected");check(gpu.snapshot()[0].position_m.z==actual[0].position_m.z,"rejected reset mutated state");
 rejected=false;try{GpuSpheres tiny(128);tiny.reset(reference);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"memory budget not enforced");
 const auto timing=gpu.timing();const auto plan=gpu_memory_plan(1591550,32);
 if(argc==2){const std::filesystem::path out(argv[1]);check(std::filesystem::create_directory(out),"output must be new");std::ofstream f(out/"result.csv");f<<std::setprecision(17)<<"device,global_bytes,max_allocation_bytes,fp64_config,max_error,upload_wall_s,upload_gpu_s,compute_wall_s,compute_gpu_s,download_wall_s,download_gpu_s,target_1591550_slots32_bytes\n"<<device.name<<','<<device.global_bytes<<','<<device.max_allocation_bytes<<','<<device.fp64_config<<','<<error<<','<<timing.upload_wall_s<<','<<timing.upload_gpu_s<<','<<timing.compute_wall_s<<','<<timing.compute_gpu_s<<','<<timing.download_wall_s<<','<<timing.download_gpu_s<<','<<plan.total_bytes<<'\n';f.close();check(bool(f),"result write");}
 std::cout<<device.name<<" FP64="<<device.fp64_config<<" max_error="<<error<<" compute_gpu="<<timing.compute_gpu_s<<" compute_wall="<<timing.compute_wall_s<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
