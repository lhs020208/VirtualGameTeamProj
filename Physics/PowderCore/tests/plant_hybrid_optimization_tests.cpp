#include "powder/plant.hpp"
#include <iostream>
#include <iomanip>
#include <cstring>
int main(int argc,char**argv){try{
 if(argc!=5&&argc!=6)throw std::runtime_error("baseline candidate probes report");
 powder::GranularConfig c;c.plant=true;c.plant_threaded=true;c.kernel_optimize=true;c.count=30;c.radius=.002f;c.dt=.00001f;c.iterations=8;
 std::vector<powder::Grain> seed;for(int i=0;i<30;++i)seed.push_back({-1.2f+.01f*(i%5),1.4f+.01f*(i/5),3,float(i%3)});
 c.plant_data=powder::read_plant_data(std::string(argv[1])+"/runtime.plant");powder::GranularGPU base(c,seed,std::vector<powder::Grain>(30));
 c.plant_wall_mode=argc==6?unsigned(std::stoul(argv[5])):0;c.plant_data=powder::read_plant_data(std::string(argv[2])+"/runtime.plant");powder::GranularGPU candidate(c,seed,std::vector<powder::Grain>(30));
 std::ifstream in(argv[3],std::ios::binary);std::vector<powder::Grain> probes;powder::Grain p;while(in.read(reinterpret_cast<char*>(&p),sizeof(p)))probes.push_back(p);if(probes.empty())throw std::runtime_error("empty probes");
 auto dist=[](auto a,auto b){return std::hypot(std::hypot(a.x-b.x,a.y-b.y),a.z-b.z);};double maxP=0,maxN=0,maxV=0;size_t count=0,different=0;
 for(float angle:{0.f,.35f,1.1f})for(bool shell:{false,true})for(size_t start=0;start<probes.size();start+=30){
  std::vector<powder::Grain> points(probes.begin()+start,probes.begin()+std::min(start+30,probes.size()));
  for(auto& q:points){if(q.w==1){float y=q.y,z=q.z-.9f;q.y=std::cos(angle)*y-std::sin(angle)*z;q.z=.9f+std::sin(angle)*y+std::cos(angle)*z;}q.w=0;}
  auto a=base.inspect_boundaries(points,angle,false,shell),b=candidate.inspect_boundaries(points,angle,false,shell);
  auto repeated=candidate.inspect_boundaries(points,angle,false,shell);for(size_t i=0;i<b.size();++i)if(dist(repeated[i].position,b[i].position)>0||dist(repeated[i].normal,b[i].normal)>0||dist(repeated[i].velocity,b[i].velocity)>0)throw std::runtime_error("memo changed query");
  for(size_t i=0;i<a.size();++i){double dp=dist(a[i].position,b[i].position);maxP=std::max(maxP,dp);maxN=std::max(maxN,double(dist(a[i].normal,b[i].normal)));maxV=std::max(maxV,double(dist(a[i].velocity,b[i].velocity)));different+=dp>1e-6;}count+=points.size();
 }
 auto stats=candidate.plant_wall_stats();std::cout<<"cache builds="<<stats[0]<<" reuses="<<stats[1]<<" overflows="<<stats[2]<<" memo="<<stats[3]<<std::endl;
 std::ofstream out(argv[4]);out<<std::setprecision(12)<<"{\"queries\":"<<count<<",\"position_delta_m\":"<<maxP<<",\"normal_delta\":"<<maxN<<",\"velocity_delta_m_s\":"<<maxV<<",\"positions_over_1um\":"<<different<<"}";
 std::cout<<"queries="<<count<<" position="<<maxP<<" normal="<<maxN<<" velocity="<<maxV<<" over1um="<<different<<std::endl;
 return maxP<=1e-6&&maxN<=1e-4&&maxV<=1e-4?0:2;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
