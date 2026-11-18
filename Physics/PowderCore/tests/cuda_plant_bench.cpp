#include "powder/plant.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
int main(int argc,char**argv){try{
 if(argc!=9)throw std::runtime_error("backend data fixture phase steps output rpm dt");
 int backend=std::stoi(argv[1]),phase=std::stoi(argv[4]);unsigned steps=std::stoul(argv[5]);std::string root=argv[2],fixture=argv[3],out=argv[6];float rpm=std::stof(argv[7]),dt=std::stof(argv[8]);
 auto d=powder::read_plant_data(root+"/runtime.plant");auto routes=powder::read_plant_routes(root+"/runtime.routes");powder::plant_set_manual_valves(d,routes,1000,3,3,phase>=1,phase>=1);powder::plant_set_receiver_valve(d,phase>=2);
 std::ifstream in(root+"/test_charge.csv");std::string line;std::getline(in,line);std::vector<powder::Grain> p;
 while(std::getline(in,line)){std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);unsigned id,type;std::string supplier;float x,y,z,r,rho;if(!(row>>id>>type>>supplier>>x>>y>>z>>r>>rho)||id!=p.size()||std::abs(r-.002f)>1e-8||rho!=1200)throw std::runtime_error("charge");p.push_back({x,y,z,float(type)});}
 powder::GranularConfig c;c.plant=true;c.plant_threaded=true;c.kernel_optimize=true;c.plant_data=d;c.count=unsigned(p.size());c.radius=.002f;c.dt=dt;c.iterations=8;c.plant_wall_mode=5;
#ifdef CUDA_COMPARISON_NATIVE
 if(backend<0||backend>4)throw std::runtime_error("backend");c.backend=static_cast<powder::GranularBackend>(backend);
#else
 if(backend!=0)throw std::runtime_error("frozen OpenCL baseline only");
#endif
 std::vector<powder::Grain> v(p.size());powder::GranularGPU gpu(c,p,v);
 if(fixture!="initial"){std::ifstream f(fixture,std::ios::binary);std::uint32_t n=0;f.read(reinterpret_cast<char*>(&n),4);if(n!=p.size())throw std::runtime_error("fixture count");f.read(reinterpret_cast<char*>(p.data()),n*sizeof(p[0]));f.read(reinterpret_cast<char*>(v.data()),n*sizeof(v[0]));if(!f||f.peek()!=EOF)throw std::runtime_error("fixture length");}
 gpu.set_state(p,v);gpu.advance(16,rpm,false);gpu.set_state(p,v);
 auto start=std::chrono::steady_clock::now();for(unsigned i=0;i<steps;i+=16)gpu.advance(std::min(16u,steps-i),rpm,false);double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 auto a=gpu.snapshot(),b=gpu.velocity_snapshot();double maxspeed=0;std::uint64_t count[2]{};for(size_t i=0;i<a.size();++i){if(a[i].w!=p[i].w||!std::isfinite(a[i].x+a[i].y+a[i].z+b[i].x+b[i].y+b[i].z))throw std::runtime_error("invalid state/group");if(a[i].w<0||a[i].w>1)throw std::runtime_error("invalid species");++count[int(a[i].w)];maxspeed=std::max(maxspeed,std::hypot(double(b[i].x),double(b[i].y),double(b[i].z)));}
 std::ofstream binary(out+".bin",std::ios::binary);auto n=c.count;binary.write(reinterpret_cast<char*>(&n),4);binary.write(reinterpret_cast<char*>(a.data()),a.size()*sizeof(a[0]));binary.write(reinterpret_cast<char*>(b.data()),b.size()*sizeof(b[0]));
 std::ofstream json(out+".json");json<<std::setprecision(12)<<"{\"backend\":"<<backend<<",\"device\":\""<<gpu.device()<<"\",\"count\":"<<n<<",\"steps\":"<<steps<<",\"dt_s\":"<<dt<<",\"phase\":"<<phase<<",\"rpm\":"<<rpm<<",\"sim_s\":"<<gpu.time()<<",\"wall_s\":"<<wall<<",\"rate\":"<<gpu.time()/wall<<",\"species\":["<<count[0]<<","<<count[1]<<"],\"max_speed_m_s\":"<<maxspeed<<",\"timing\":";powder::write_granular_timing(json,gpu.timing());json<<"}";
 std::cout<<"backend="<<backend<<" sim_s="<<gpu.time()<<" wall_s="<<wall<<" rate="<<gpu.time()/wall<<"\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
