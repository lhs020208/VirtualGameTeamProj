#include "powder/gpu.hpp"
#include <cmath>
#include <iostream>
#include <chrono>
#include <fstream>
using namespace powder;
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
std::vector<Particle> particles(unsigned n){std::vector<Particle> p;for(unsigned i=0;i<n;++i){Particle a;a.id=i+1;a.diameter_m=.002;a.material.density_kg_m3=1500;unsigned j=n-1-i;a.position_m={double(j%17)*.0019999-.016,double((j/17)%17)*.0021-.016,double(j/289)*.0021+.01};a.velocity_m_s={i%2?.02:-.02,.01,0};p.push_back(a);}return p;}
void compare(GpuSpheres& a,GpuSpheres& b){auto x=a.snapshot(),y=b.snapshot();check(x.size()==y.size(),"size");for(std::size_t i=0;i<x.size();++i){check(x[i].id==y[i].id,"ID");for(auto values:{std::pair<Vec3,Vec3>{x[i].position_m,y[i].position_m},{x[i].velocity_m_s,y[i].velocity_m_s},{x[i].angular_velocity_rad_s,y[i].angular_velocity_rad_s}})check(values.first.x==values.second.x&&values.first.y==values.second.y&&values.first.z==values.second.z,"sort changed state");}check(a.contact_pairs()==b.contact_pairs(),"sort changed contacts");}
int main(int argc,char** argv){try{MaterialContact m;m.law=ContactLaw::Linear;m.linear={10,0,2.5,0,.4};
 for(unsigned n:{1u,127u,128u,129u,255u,256u,257u,1025u,32769u}){
  auto p=particles(n);GpuSpheres reference,pruned,fused;for(auto* g:{&reference,&pruned,&fused}){g->reset(p);g->enable_contacts(m);}reference.set_sort_optimizations(false,false);pruned.set_sort_optimizations(true,false);
  for(unsigned w=0;w<5;++w){for(auto* g:{&reference,&pruned,&fused})g->advance_contacts(1e-6,20,{});compare(reference,pruned);compare(reference,fused);}
  // Forced rebuild every sort, including crossing from current to predicted positions.
  reference.set_spatial_reuse(false);fused.set_spatial_reuse(false);for(unsigned w=0;w<3;++w){reference.advance_contacts(1e-6,20,{});fused.advance_contacts(1e-6,20,{});compare(reference,fused);}
 }
 // Failure with a multi-workgroup sort must preserve the same accepted prefix.
 for(unsigned accepted:{1u,2u}){auto p=particles(129);p[0].position_m={1048576.*.002-(accepted+.5)*1e-7,0,0};p[0].velocity_m_s={1,0,0};GpuSpheres reference,fused;for(auto* g:{&reference,&fused}){g->reset(p);g->enable_contacts(m);}reference.set_sort_optimizations(false,false);
 for(auto* g:{&reference,&fused}){bool rejected=false;try{g->advance_contacts(1e-7,5,{});}catch(const std::runtime_error&){rejected=true;}check(rejected&&std::abs(g->physical_time_s()-accepted*1e-7)<1e-20,"multiworkgroup prefix");}compare(reference,fused);for(auto* g:{&reference,&fused})g->advance_contacts(1e-9,1,{});compare(reference,fused);}
 if(argc==2){std::ofstream out(argv[1]);out<<"count,force_rebuild,variant,repeat,seconds,sort_gpu_s\n";for(unsigned n:{129u,32769u})for(bool force:{false,true})for(unsigned repeat=0;repeat<3;++repeat)for(unsigned order=0;order<3;++order){unsigned variant=(order+repeat)%3;GpuSpheres g;g.reset(particles(n));g.enable_contacts(m);g.set_sort_optimizations(variant>=1,variant>=2);g.set_spatial_reuse(!force);g.set_profiling(true);auto start=std::chrono::steady_clock::now();g.advance_contacts(1e-7,12,{});double duration=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();out<<n<<','<<force<<','<<variant<<','<<repeat<<','<<duration<<','<<g.timing().sort_gpu_s<<'\n';}check(bool(out),"benchmark write");}
 std::cout<<"PASS 25/26 exact reference/pruned/fused parity, padding boundaries, reversed IDs, forced rebuild\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
