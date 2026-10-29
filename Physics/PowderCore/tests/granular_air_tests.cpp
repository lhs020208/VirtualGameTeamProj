#include "powder/granular.hpp"
#include <iostream>
#include <cmath>
void must(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){try{
 powder::GranularConfig c;c.binary_materials=true;c.count=1;c.dt=.000375f;
 c.materials[0].radius_m=.006;c.materials[0].density_kg_m3=900;c.materials[1].radius_m=.005;c.materials[1].density_kg_m3=1500;
 c.air.enabled=true;c.air.velocity_m_s={-2,.5,1};
 std::vector<powder::Grain> initial{{.3f,.18f,.35f,0}},v{{.1f,0,0,0}};
 for(int group=0;group<2;++group){initial[0].w=float(group);
  for(bool optimized:{false,true}){c.candidates=optimized;c.candidate_reuse=optimized;c.kernel_optimize=optimized;c.candidate_audit=optimized;
   powder::GranularGPU gpu(c,initial,v);gpu.advance(200,0,false);
   auto a=gpu.velocity_snapshot()[0];auto p=gpu.snapshot()[0];std::array<double,3> ref{.1f,0,0},pos{initial[0].x,initial[0].y,initial[0].z};
   for(int i=0;i<200;++i){ref=powder::advance_sphere_air(c.air,c.materials[group].radius_m,c.materials[group].density_kg_m3,ref,c.dt);for(int k=0;k<3;++k)pos[k]+=ref[k]*c.dt;}
   must(std::abs(a.x-ref[0])<2.e-5&&std::abs(a.y-ref[1])<2.e-5&&std::abs(a.z-ref[2])<2.e-5,"CPU/GPU drag velocity mismatch");
   must(std::abs(p.x-pos[0])<1.e-5&&std::abs(p.z-pos[2])<1.e-5,"CPU/GPU drag position mismatch");must(p.w==group,"group changed");
  }
 }
 // GPU time refinement against a much finer double reference (same physical end time).
 auto fine=c;fine.candidates=false;fine.candidate_reuse=false;fine.kernel_optimize=false;fine.candidate_audit=false;
 fine.materials[0].density_kg_m3=10;initial[0].w=0;
 std::array<double,3> ref{.1f,0,0};for(int i=0;i<4000;++i)ref=powder::advance_sphere_air(fine.air,.006,10,ref,.02/4000);
 double errors[2];for(int k=0;k<2;++k){fine.dt=k?.0005f:.001f;powder::GranularGPU gpu(fine,initial,v);gpu.advance(k?40:20,0,false);auto q=gpu.velocity_snapshot()[0];errors[k]=std::hypot(q.x-ref[0],q.z-ref[2]);}
 must(errors[1]<errors[0]*.7,"GPU timestep refinement");
 // Air must oppose falling motion rather than accelerate it.

 c.candidates=false;c.candidate_reuse=false;c.kernel_optimize=false;c.candidate_audit=false;c.air.velocity_m_s={0,0,0};initial[0].w=0;
 c.air.enabled=false;powder::GranularGPU dry(c,initial,v);dry.advance(100,0,false);auto dryv=dry.velocity_snapshot()[0];
 c.air.enabled=true;powder::GranularGPU wet(c,initial,v);wet.advance(100,0,false);auto wetv=wet.velocity_snapshot()[0];must(wetv.x<dryv.x&&wetv.z>dryv.z,"air must decelerate falling sphere");
 c.materials[0].density_kg_m3=c.air.density_kg_m3;powder::GranularGPU neutral(c,initial,{{0,0,0,0}});neutral.advance(100,0,false);must(std::abs(neutral.velocity_snapshot()[0].z)<1.e-7,"neutral buoyancy GPU");
 c.materials[0].density_kg_m3=.001;powder::GranularGPU stiff(c,initial,{{0,0,0,0}});bool rejected=false;try{stiff.advance(1,0,false);}catch(const std::exception& e){rejected=std::string(e.what()).find("32")!=std::string::npos;}must(rejected,"unresolved air step must reject");
 c.materials[0].density_kg_m3=900;c.materials[0].source_diameter_m=1.e-4;rejected=false;try{powder::GranularGPU invalid(c);}catch(const std::exception&){rejected=true;}must(rejected,"coarse source cannot silently drive drag");
 std::cout<<"PASS GPU air CPU parity both materials/paths, gravity/drag, buoyancy, safety, coarse-grain rejection\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
