#include "powder/plant.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
// Diagnostic only: stationary surface probes, never modifies runtime assets.
int main(int argc,char**argv){try{
 if(argc!=4&&argc!=5)throw std::runtime_error("data-dir probes-text output-csv");
 powder::GranularConfig c;c.plant=true;c.plant_threaded=true;c.kernel_optimize=true;c.profile=true;c.count=30;c.radius=.002f;c.dt=.00001f;c.iterations=8;
 c.plant_data=powder::read_plant_data(std::string(argv[1])+"/runtime.plant");
 for(int j=0;j<int(c.plant_data[5].y);++j){int k=int(c.plant_data[5].x)+2*j;c.plant_data[k+1].y=1;c.plant_data[k+1].z=1;}
 std::vector<powder::Grain> seed;for(int i=0;i<30;++i)seed.push_back({-1.2f+.01f*(i%5),1.4f+.01f*(i/5),3,0});
 if(argc==5){std::ifstream mask(argv[4]);if(!mask)throw std::runtime_error("mask file");int k,type;while(mask>>k>>type){if(k<16||k+3>=int(c.plant_data.size()))throw std::runtime_error("mask index");for(int j=0;j<(type==1?3:1);++j)c.plant_data[k+j].x+=1000.f;}}
 powder::GranularGPU gpu(c,seed,std::vector<powder::Grain>(30));
 std::ifstream in(argv[2]);std::ofstream out(argv[3]);if(!in||!out)throw std::runtime_error("files");
 out<<"region,angle,points,gpu_us_median,gpu_us_min,gpu_us_max\n"<<std::setprecision(10);
 std::string name;int n;
 while(in>>name>>n){std::vector<powder::Grain> points(n);for(auto& p:points)if(!(in>>p.x>>p.y>>p.z))throw std::runtime_error("probe data");
  for(float angle:{0.f,.35f}){auto sample=points;if(name=="MX-301_Collision_Rotor")for(auto& p:sample){float y=p.y,z=p.z-.9f;p.y=std::cos(angle)*y-std::sin(angle)*z;p.z=.9f+std::sin(angle)*y+std::cos(angle)*z;}
   gpu.inspect_boundaries(sample,angle);std::vector<double> times;
   for(int trial=0;trial<5;++trial){auto before=gpu.timing();for(int repeat=0;repeat<12;++repeat)gpu.inspect_boundaries(sample,angle);auto after=gpu.timing();times.push_back((after.gpu_seconds[13]-before.gpu_seconds[13])*1e6/12);}
   std::sort(times.begin(),times.end());out<<name<<','<<angle<<','<<n<<','<<times[2]<<','<<times.front()<<','<<times.back()<<'\n';out.flush();
  }
 }
 std::cout<<"PASS wall profile "<<gpu.device()<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
