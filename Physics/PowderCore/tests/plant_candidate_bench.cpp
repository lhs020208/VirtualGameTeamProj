#include "powder/plant.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
int main(int argc,char**argv){try{
 if(argc!=9&&argc!=10)throw std::runtime_error("data count checkpoint-or-initial mode steps profile-split output rpm [same-input-wall-audit]");
 std::string root=argv[1],prefix=argv[7];unsigned n=std::stoul(argv[2]),steps=std::stoul(argv[5]);
 auto d=powder::read_plant_data(root+"/runtime.plant");powder::plant_set_manual_valves(d,powder::read_plant_routes(root+"/runtime.routes"),1000,3,3,true,true);powder::plant_set_receiver_valve(d,false);
 std::ifstream in(root+"/test_charge.csv");std::string line;std::getline(in,line);std::vector<powder::Grain> all;
 while(std::getline(in,line)){std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);unsigned id,type;std::string supplier;float x,y,z,r,rho;if(!(row>>id>>type>>supplier>>x>>y>>z>>r>>rho)||std::abs(r-.002f)>1e-7||rho!=1200)throw std::runtime_error("charge");all.push_back({x,y,z,float(type)});}
 if(n>all.size()||!n)throw std::runtime_error("count");std::vector<powder::Grain> p,v(n);for(unsigned i=0;i<n;++i)p.push_back(all[size_t(i)*all.size()/n]);
 powder::GranularConfig c;c.plant=true;c.plant_threaded=true;c.kernel_optimize=true;c.plant_data=d;c.count=n;c.radius=.002f;c.dt=.00001f;c.iterations=8;c.plant_wall_mode=std::stoul(argv[4]);int profile=std::stoi(argv[6]);c.profile=profile!=0;c.profile_split=profile==2;c.plant_boundary_audit=argc==10&&std::stoi(argv[9])!=0;
 powder::GranularGPU gpu(c,p,v);
 if(std::string(argv[3])!="initial"){std::ifstream f(argv[3],std::ios::binary);std::uint32_t count;f.read(reinterpret_cast<char*>(&count),4);if(count!=n)throw std::runtime_error("checkpoint count");f.read(reinterpret_cast<char*>(p.data()),n*sizeof(p[0]));f.read(reinterpret_cast<char*>(v.data()),n*sizeof(v[0]));if(!f)throw std::runtime_error("checkpoint");}
 float rpm=std::stof(argv[8]);gpu.set_state(p,v);gpu.advance(16,rpm,false);gpu.set_state(p,v);
 auto start=std::chrono::steady_clock::now();for(unsigned i=0;i<steps;i+=16)gpu.advance(std::min(16u,steps-i),rpm,false);double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 auto a=gpu.snapshot(),b=gpu.velocity_snapshot();std::ofstream binary(prefix+".bin",std::ios::binary);binary.write(reinterpret_cast<char*>(&n),4);binary.write(reinterpret_cast<char*>(a.data()),n*sizeof(a[0]));binary.write(reinterpret_cast<char*>(b.data()),n*sizeof(b[0]));
 auto mem=gpu.plant_memory_bytes();std::ofstream memory(prefix+".memory.json");memory<<"{\"plant_bytes\":"<<mem[0]<<",\"acceleration_bytes\":"<<mem[1]<<",\"cache_bytes\":"<<mem[2]<<"}";auto stats=gpu.plant_wall_stats();std::ofstream out(prefix+".json");out<<std::setprecision(12)<<"{\"count\":"<<n<<",\"steps\":"<<steps<<",\"mode\":"<<c.plant_wall_mode<<",\"wall_s\":"<<wall<<",\"rate\":"<<steps*c.dt/wall<<",\"cache\":["<<stats[0]<<","<<stats[1]<<","<<stats[2]<<","<<stats[3]<<"],\"timing\":";powder::write_granular_timing(out,gpu.timing());out<<"}";
 std::cout<<prefix<<" rate="<<steps*c.dt/wall<<" wall="<<wall<<std::endl;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
