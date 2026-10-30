#include "powder/plant.hpp"
#include <sstream>
#include <iostream>
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("section root required");std::string root=argv[1];auto d=powder::read_plant_data(root+"/runtime.plant");int section=int(d[11].x);if(section<1||section>4||d[10].x!=0)throw std::runtime_error("not source-mesh section");auto routes=powder::read_plant_routes(root+"/runtime.routes");
 powder::plant_set_manual_valves(d,routes,1000,0,0,false,false);for(auto&r:routes)for(int at:r.cells)if(d[at+2].x!=0)throw std::runtime_error("closed flow");
 powder::plant_set_manual_valves(d,routes,1000,7,7,true,true);for(auto&r:routes)for(int at:r.cells){bool expected=(section==1&&r.phase==0)||(section==2&&r.phase==1);if((d[at+2].x>0)!=expected)throw std::runtime_error("section flow leakage");}
 powder::plant_set_receiver_valve(d,section==3);
 std::ifstream f(root+"/test_charge.csv");std::string line;std::getline(f,line);std::vector<powder::Grain> initial;
 while(std::getline(f,line)){std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);int id,type;std::string supplier;float x,y,z,r,rho;if(!(row>>id>>type>>supplier>>x>>y>>z>>r>>rho)||id!=initial.size())throw std::runtime_error("charge");initial.push_back({x,y,z,float(type)});}
 if(initial.size()!=3000)throw std::runtime_error("count");powder::GranularConfig c;c.plant=true;c.plant_threaded=true;c.kernel_optimize=true;c.plant_data=d;c.count=3000;c.radius=.00125f;c.dt=.00001f;c.iterations=8;powder::GranularGPU gpu(c,initial,std::vector<powder::Grain>(3000));auto hits=gpu.inspect_boundaries(initial,0,false,false);float max=0;
 for(size_t i=0;i<initial.size();++i){auto a=initial[i],b=hits[i].position;float move=std::hypot(std::hypot(a.x-b.x,a.y-b.y),a.z-b.z);max=std::max(max,move);if(move>.00002f)throw std::runtime_error("initial particle intersects source geometry");}
 gpu.advance(320,section>=3?15.f:0.f,false);auto result=gpu.snapshot();unsigned counts[3]{};for(auto a:result){if(!std::isfinite(a.x+a.y+a.z)||a.w<0||a.w>2)throw std::runtime_error("state");++counts[int(a.w)];}for(auto n:counts)if(n!=1000)throw std::runtime_error("species count");
 std::cout<<"PASS section="<<section<<" initial_max_correction="<<max<<" no wrong-route flow; 3000 particles; 320 steps"<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
