#include "powder/demo_plant.hpp"
#include "powder/plant_pressure.hpp"
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
int main(int argc,char**argv){try{
 std::string root=argc>1?argv[1]:".";double duration=argc>2?std::stod(argv[2]):10;unsigned iterations=argc>3?unsigned(std::stoi(argv[3])):1;
 std::string data=root+"/SourceAssets/ProcessPlant_20261111_LowPoly/Simulation/";
 powder::GranularConfig c;c.plant_data=powder::read_plant_data(data+"runtime.plant");c.dt=1.f/120;c.radius=.002f;c.iterations=iterations;
 auto routes=powder::read_plant_routes(data+"runtime.routes");std::ifstream f(data+"test_charge.csv");std::string line;std::getline(f,line);std::vector<powder::Grain> p;
 while(std::getline(f,line)){std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);unsigned id,type;std::string supplier;float x,y,z,r,rho;row>>id>>type>>supplier>>x>>y>>z>>r>>rho;p.push_back({x,y,z,float(type)});}
 powder::DemoPlant sim(c,p,routes,root+"/SourceAssets/ProcessPlant_Demo20261119/rotor_boxes.txt");
 double worst=0,nextDump=0;auto start=std::chrono::steady_clock::now();int control=-1;std::ofstream dump;if(argc>4)dump.open(argv[4],std::ios::binary);
 while(sim.time()<duration){int phase=sim.time()<2?0:sim.time()<22?1:sim.time()<48?2:3;if(control!=phase){powder::plant_set_manual_valves(c.plant_data,routes,1000,phase>=1?3:0,3,phase>=2,phase>=2);powder::plant_set_receiver_valve(c.plant_data,phase>=3);sim.update_plant_field(c.plant_data);control=phase;}
  sim.advance(1,control>=3?15.f:0.f,false);worst=std::max(worst,sim.max_containment_error());
  if(control==0){auto n=sim.counts();if(n[6]||n[7]||n[8]||n[9])throw std::runtime_error("closed source valve leaked");}
  if(dump&&sim.time()>=nextDump){auto state=sim.snapshot();double t=sim.time();std::uint32_t n=unsigned(state.size());dump.write(reinterpret_cast<char*>(&t),8);dump.write(reinterpret_cast<char*>(&n),4);dump.write(reinterpret_cast<char*>(state.data()),state.size()*sizeof(powder::Grain));nextDump+=5;}
 }
 double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();auto counts=sim.counts();auto final=sim.snapshot();unsigned a=0,b=0;for(auto q:final){if(q.w==0)++a;else if(q.w==1)++b;else throw std::runtime_error("species changed");}
 std::cout<<std::setprecision(12)<<"{\"physical_s\":"<<sim.time()<<",\"wall_s\":"<<wall<<",\"rate\":"<<sim.time()/wall<<",\"count\":"<<final.size()<<",\"species_a\":"<<a<<",\"species_b\":"<<b<<",\"max_containment_m\":"<<worst<<",\"zones\":[";for(int i=0;i<10;++i)std::cout<<(i?",":"")<<counts[i];std::cout<<"]}\n";
 return worst>1e-5?2:0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
