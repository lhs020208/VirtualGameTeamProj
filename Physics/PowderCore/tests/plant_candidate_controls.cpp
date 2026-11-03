#include "powder/plant.hpp"
#include <iostream>
#include <iomanip>
int main(int argc,char**argv){try{
 if(argc!=4)throw std::runtime_error("data mode report");std::string root=argv[1];auto d=powder::read_plant_data(root+"/runtime.plant");auto routes=powder::read_plant_routes(root+"/runtime.routes");
 powder::GranularConfig c;c.plant=true;c.plant_threaded=true;c.kernel_optimize=true;c.plant_data=d;c.count=30;c.radius=.002f;c.dt=.00001f;c.iterations=8;
 std::vector<powder::Grain> seed;for(int i=0;i<30;++i)seed.push_back({-1.2f+.01f*(i%5),1.4f+.01f*(i/5),3,float(i%2)});
 powder::GranularGPU base(c,seed,std::vector<powder::Grain>(30));c.plant_wall_mode=std::stoul(argv[2]);powder::GranularGPU candidate(c,seed,std::vector<powder::Grain>(30));
 std::ifstream f(root+"/surface_probes.bin",std::ios::binary);std::vector<powder::Grain> probes;powder::Grain p;while(f.read(reinterpret_cast<char*>(&p),sizeof(p)))probes.push_back(p);if(probes.empty())throw std::runtime_error("probes");
 auto distance=[](auto a,auto b){return std::hypot(std::hypot(a.x-b.x,a.y-b.y),a.z-b.z);};double dp=0,dn=0,dv=0;size_t queries=0;std::uint64_t overflow=0,memo=0;
 for(int state=0;state<6;++state){
  powder::plant_set_manual_valves(d,routes,1000,state%2?7:0,7,state%3!=0,true);powder::plant_set_receiver_valve(d,state%2!=0);base.update_plant_field(d);candidate.update_plant_field(d);
  std::vector<powder::Grain> all;for(size_t i=state;i<probes.size();i+=17){auto q=probes[i];if(q.w==1){float y=q.y,z=q.z-.9f,a=state*.41f;q.y=std::cos(a)*y-std::sin(a)*z;q.z=.9f+std::sin(a)*y+std::cos(a)*z;}q.w=0;all.push_back(q);}
  // Exact gate centres/rims, penetrations, and large jumps between cache queries.
  for(int j=0;j<int(d[5].y);++j){auto g=d[int(d[5].x)+2*j];all.push_back({g.x,g.y,g.z+.001f,0});all.push_back({g.x+g.w,g.y,g.z,0});}
  all.push_back({2.17f,0,2.617f,0});all.push_back({2.69f,0,2.619f,0});all.push_back({7,7,7,0});
  for(bool shell:{false,true})for(size_t begin=0;begin<all.size();begin+=30){std::vector<powder::Grain> points(all.begin()+begin,all.begin()+std::min(begin+30,all.size()));auto a=base.inspect_boundaries(points,state*.41f,false,shell),b=candidate.inspect_boundaries(points,state*.41f,false,shell);auto repeat=candidate.inspect_boundaries(points,state*.41f,false,shell);
   for(size_t i=0;i<a.size();++i){dp=std::max(dp,double(distance(a[i].position,b[i].position)));dn=std::max(dn,double(distance(a[i].normal,b[i].normal)));dv=std::max(dv,double(distance(a[i].velocity,b[i].velocity)));if(distance(b[i].position,repeat[i].position)||distance(b[i].normal,repeat[i].normal)||distance(b[i].velocity,repeat[i].velocity))throw std::runtime_error("memo mismatch");}queries+=points.size();
  }auto stats=candidate.plant_wall_stats();overflow+=stats[2];memo+=stats[3];
 }
 // Updating the original field after acceleration upload must preserve new pointers.
 candidate.set_state(seed,std::vector<powder::Grain>(30));base.set_state(seed,std::vector<powder::Grain>(30));candidate.advance(128,15,false);base.advance(128,15,false);auto a=base.snapshot(),b=candidate.snapshot();for(size_t i=0;i<a.size();++i)dp=std::max(dp,double(distance(a[i],b[i])));
 auto mem=candidate.plant_memory_bytes();std::ofstream out(argv[3]);out<<std::setprecision(12)<<"{\"queries\":"<<queries<<",\"position_delta_m\":"<<dp<<",\"normal_delta\":"<<dn<<",\"wall_velocity_delta\":"<<dv<<",\"overflow_fallbacks\":"<<overflow<<",\"memo_hits\":"<<memo<<",\"plant_bytes\":"<<mem[0]<<",\"acceleration_bytes\":"<<mem[1]<<",\"cache_bytes\":"<<mem[2]<<"}";
 std::cout<<"controls queries="<<queries<<" delta="<<dp<<","<<dn<<","<<dv<<" overflows="<<overflow<<std::endl;return dp==0&&dn==0&&dv==0?0:2;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
