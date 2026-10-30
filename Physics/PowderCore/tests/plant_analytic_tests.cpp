#include "powder/plant.hpp"
#include <iostream>
#include <fstream>
int main(int argc,char**argv){try{
#ifndef POWDER_PLANT_ANALYTIC_EXPERIMENT
 std::cerr<<"Analytic experiment disabled; apply documented rejected-candidate patch in an isolated experiment first."<<std::endl;return 3;
#endif
 if(argc!=4)throw std::runtime_error("base-data candidate-data output");auto d=powder::read_plant_data(std::string(argv[1])+"/runtime.plant"),o=powder::read_plant_data(std::string(argv[2])+"/runtime.plant");
 powder::GranularConfig c;c.plant=true;c.plant_threaded=true;c.kernel_optimize=true;c.count=1;c.radius=.00125f;c.dt=.00001f;c.iterations=8;c.plant_data=d;powder::GranularGPU base(c,{{-1.2f,1.4f,3,0}},{{0,0,0,0}});c.plant_data=o;powder::GranularGPU opt(c,{{-1.2f,1.4f,3,0}},{{0,0,0,0}});
 double maxDelta=0,maxNormal=0,maxResidual=0,maxFallback=0;unsigned count=0;powder::Grain worst{},worstBase{},worstOpt{};
 for(float angle:{0.f,.3f,1.1f}){
 std::vector<powder::Grain> probes;for(float z:{.699f,.700f,.705f,.70999f,.71001f,.72f,.75f,.8f,.85f,.9f,.93999f,.94001f,.945f,.95f,.951f})for(int j=0;j<512;++j)for(float offset:{-.003f,-.001f,-.00002f,.00002f,.001f}){float radius=.022f+(z-.7f)*1.312f+offset,a=j*6.28318530718f/512+(angle>.1f?3.14159265359f/512:0.f);probes.push_back({radius*std::cos(a),radius*std::sin(a),z,0});}
 auto a=base.inspect_boundaries(probes,angle,false,false),b=opt.inspect_boundaries(probes,angle,false,false);std::vector<powder::Grain> corrected;for(auto v:b)corrected.push_back(v.position);auto recheck=base.inspect_boundaries(corrected,angle,false,false);
 auto dist=[](powder::Grain a,powder::Grain b){return double(std::hypot(std::hypot(a.x-b.x,a.y-b.y),a.z-b.z));};
 for(size_t i=0;i<a.size();++i){double delta=dist(a[i].position,b[i].position);if(delta>maxDelta){maxDelta=delta;worst=probes[i];worstBase=a[i].position;worstOpt=b[i].position;}maxResidual=std::max(maxResidual,dist(b[i].position,recheck[i].position));if(dist(a[i].position,probes[i])>1e-5&&dist(b[i].position,probes[i])>1e-5)maxNormal=std::max(maxNormal,dist(a[i].normal,b[i].normal));if(probes[i].z<=.71f||probes[i].z>=.94f)maxFallback=std::max(maxFallback,delta);}count+=unsigned(a.size());
 }
 bool pass=maxDelta<=.00002&&maxResidual<=.00002&&maxFallback==0&&maxNormal<.02;
 std::ofstream out(argv[3]);out<<"{\"samples\":"<<count<<",\"max_position_delta_m\":"<<maxDelta<<",\"max_original_wall_residual_m\":"<<maxResidual<<",\"max_normal_vector_delta\":"<<maxNormal<<",\"max_rim_fallback_delta_m\":"<<maxFallback<<",\"pass\":"<<(pass?"true":"false")<<"}";
 std::cout<<"WORST p="<<worst.x<<","<<worst.y<<","<<worst.z<<" base="<<worstBase.x<<","<<worstBase.y<<","<<worstBase.z<<" candidate="<<worstOpt.x<<","<<worstOpt.y<<","<<worstOpt.z<<std::endl;
 std::cout<<"BOUNDARY "<<count<<" pos="<<maxDelta<<" normal="<<maxNormal<<" residual="<<maxResidual<<" rim="<<maxFallback<<" pass="<<pass<<std::endl;return pass?0:2;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
