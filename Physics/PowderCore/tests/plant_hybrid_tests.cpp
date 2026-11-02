#include "powder/plant.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
int main(int argc,char**argv){try{
 if(argc!=4&&argc!=5)throw std::runtime_error("reference hybrid report");std::string dir=argv[2];auto src=powder::read_plant_data(std::string(argv[1])+"/runtime.plant"),hyb=powder::read_plant_data(dir+"/runtime.plant");
 auto open=[](std::vector<powder::Grain>&d){for(int j=0;j<int(d[5].y);++j){int k=int(d[5].x)+2*j;d[k+1].y=1;d[k+1].z=1;}};open(src);open(hyb);
 powder::GranularConfig c;c.plant=true;c.plant_threaded=true;c.kernel_optimize=true;c.count=1;c.radius=.002f;c.dt=.00001f;c.iterations=8;c.plant_data=src;powder::GranularGPU base(c,{{-1.2f,1.4f,3,0}},{{0,0,0,0}});c.plant_wall_mode=argc==5?unsigned(std::stoul(argv[4])):0;c.plant_data=hyb;powder::GranularGPU fast(c,{{-1.2f,1.4f,3,0}},{{0,0,0,0}});
 // Same leaf order without spatial culling is the oracle for missed candidates.
 auto all=hyb;all[13].z=all[13].w=0;for(int tree=0;tree<2;++tree)for(int at=int(tree?all[12].y:all[12].x);at<int(tree?all[13].y:all[13].x);at+=2){all[at].x=all[at].y=all[at].z=-100;all[at+1].x=all[at+1].y=all[at+1].z=100;}c.plant_data=all;c.plant_wall_mode=0;powder::GranularGPU exhaustive(c,{{-1.2f,1.4f,3,0}},{{0,0,0,0}});
 std::ifstream f(dir+"/surface_probes.bin",std::ios::binary);std::vector<powder::Grain> samples;powder::Grain v;while(f.read(reinterpret_cast<char*>(&v),sizeof(v)))samples.push_back(v);if(samples.empty())throw std::runtime_error("no probes");
 auto dist=[](auto a,auto b){return std::hypot(std::hypot(a.x-b.x,a.y-b.y),a.z-b.z);};double maxDelta=0,maxMixer=0,maxResidual=0,maxCull=0;unsigned compared=0;
 for(float angle:{0.f,.35f,1.1f}){
  std::vector<powder::Grain> probes;for(size_t i=0;i<samples.size();i+=11){auto a=samples[i];if(a.w==1){float y=a.y,z=a.z-.9f;a.y=std::cos(angle)*y-std::sin(angle)*z;a.z=.9f+std::sin(angle)*y+std::cos(angle)*z;}a.w=0;probes.push_back(a);}
  auto a=base.inspect_boundaries(probes,angle),b=fast.inspect_boundaries(probes,angle);std::vector<powder::Grain> corrected;for(auto h:b)corrected.push_back(h.position);auto again=base.inspect_boundaries(corrected,angle);
  for(size_t i=0;i<a.size();++i){double delta=dist(a[i].position,b[i].position);if(delta>maxDelta){maxDelta=delta;if(delta>.001)std::cout<<"worst surface "<<probes[i].x<<","<<probes[i].y<<","<<probes[i].z<<" base "<<a[i].position.x<<","<<a[i].position.y<<","<<a[i].position.z<<" hybrid "<<b[i].position.x<<","<<b[i].position.y<<","<<b[i].position.z<<std::endl;}if(probes[i].x>2.15f&&probes[i].z<1.32f)maxMixer=std::max(maxMixer,delta);maxResidual=std::max(maxResidual,double(dist(b[i].position,again[i].position)));}compared+=unsigned(a.size());
  std::vector<powder::Grain> audit;for(size_t i=0;i<probes.size();i+=std::max(size_t(1),probes.size()/24))audit.push_back(probes[i]);auto x=fast.inspect_boundaries(audit,angle),y=exhaustive.inspect_boundaries(audit,angle);for(size_t i=0;i<x.size();++i)maxCull=std::max(maxCull,double(dist(x[i].position,y[i].position)));
 }
 // Every gas-cell centre along every connection must remain unobstructed.
 std::vector<powder::Grain> path;for(int j=0;j<int(hyb[0].w);++j){int k=int(hyb[0].z)+3*j;auto a=hyb[k],b=hyb[k+1];for(int n=0;n<3;++n){float t=n*.5f;path.push_back({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,0});}}
 auto pa=base.inspect_boundaries(path,0),pb=fast.inspect_boundaries(path,0);double pathDelta=0;for(size_t i=0;i<path.size();++i){double delta=dist(pa[i].position,pb[i].position);if(delta>pathDelta){pathDelta=delta;if(delta>.00002)std::cout<<"worst path "<<path[i].x<<","<<path[i].y<<","<<path[i].z<<" base "<<pa[i].position.x<<","<<pa[i].position.y<<","<<pa[i].position.z<<" hybrid "<<pb[i].position.x<<","<<pb[i].position.y<<","<<pb[i].position.z<<std::endl;}}

 // A reproducible 30-particle mixer fixture, clear of both boundary models.
 std::vector<powder::Grain> grid;for(int x=0;x<25;++x)for(int y=0;y<7;++y)for(int z=0;z<10;++z)grid.push_back({2.38f+.05f*x,-.15f+.05f*y,.67f+.05f*z,0});
 auto ga=base.inspect_boundaries(grid,0),gb=fast.inspect_boundaries(grid,0);std::vector<powder::Grain> initial;
 for(size_t j=0;j<grid.size()&&initial.size()<30;++j){size_t i=(j*173)%grid.size();if(dist(ga[i].position,grid[i])>1e-6||dist(gb[i].position,grid[i])>1e-6)continue;auto v=grid[i];v.w=float(initial.size()%3);initial.push_back(v);}
 if(initial.size()!=30)throw std::runtime_error("cannot seed mixer");c.count=30;c.plant_wall_mode=argc==5?unsigned(std::stoul(argv[4])):0;c.plant_data=hyb;powder::GranularGPU mixer(c,initial,std::vector<powder::Grain>(30));for(int step=0;step<10000;step+=16)mixer.advance(16,15,false);auto final=mixer.snapshot();unsigned species[3]{};for(auto p:final){if(!std::isfinite(p.x+p.y+p.z)||p.w<0||p.w>2)throw std::runtime_error("mixer state");++species[int(p.w)];}for(auto n:species)if(n!=10)throw std::runtime_error("mixer count");
 std::cout<<"PASS mixer 30 particles, 0.1 physical seconds, 15 RPM; particle counts preserved"<<std::endl;
 std::ofstream o(argv[3]);o<<"{\"surface_samples\":"<<compared<<",\"max_free_side_reference_position_delta_m\":"<<maxDelta<<",\"max_mixer_position_delta_m\":"<<maxMixer<<",\"max_reference_reprojection_m\":"<<maxResidual<<",\"max_bvh_vs_exhaustive_m\":"<<maxCull<<",\"path_samples\":"<<path.size()<<",\"max_path_reference_delta_m\":"<<pathDelta<<"}";
 std::cout<<"surface="<<maxDelta<<" mixer="<<maxMixer<<" residual="<<maxResidual<<" cull="<<maxCull<<" path="<<pathDelta<<std::endl;return maxCull<=.000001&&pathDelta<=.00002?0:2;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
