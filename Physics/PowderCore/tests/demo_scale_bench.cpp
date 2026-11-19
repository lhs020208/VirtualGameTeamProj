#include "powder/demo_plant.hpp"
#include "powder/plant_pressure.hpp"
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
int main(int argc,char**argv){try{
 using Clock=std::chrono::steady_clock;
 std::string root=argv[1],charge=argv[2];double duration=std::stod(argv[3]),limit=std::stod(argv[4]);
 std::string data=root+"/SourceAssets/ProcessPlant_20261111_LowPoly/Simulation/";
 powder::GranularConfig c;c.plant_data=powder::read_plant_data(data+"runtime.plant");c.dt=1.f/120;c.radius=.002f;c.iterations=1;
 auto routes=powder::read_plant_routes(data+"runtime.routes");std::ifstream f(charge);std::string line;std::getline(f,line);std::vector<powder::Grain> p;
 while(std::getline(f,line)){std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);unsigned id,type;std::string supplier;float x,y,z,r,rho;if(!(row>>id>>type>>supplier>>x>>y>>z>>r>>rho))throw std::runtime_error("invalid charge");p.push_back({x,y,z,float(type)});}
 if(p.empty())throw std::runtime_error("empty charge");
 powder::DemoPlant sim(c,p,routes,root+"/SourceAssets/ProcessPlant_Demo20261119/rotor_boxes.txt");
 double costs[4]{},snapshot=0,worst=0,next=0;unsigned steps[4]{},snapshots=0;int phase=-1;auto begin=Clock::now();
 while(sim.time()<duration&&std::chrono::duration<double>(Clock::now()-begin).count()<limit){
  int now=sim.time()<2?0:sim.time()<22?1:sim.time()<48?2:3;
  if(now!=phase){phase=now;powder::plant_set_regulated_pressures(c.plant_data,routes,powder::plant_pressure_preset(phase==1?1:phase==2?3:phase==3?4:0),phase==1?3:0,phase==1?3:0,phase==2,phase==2,phase==3);sim.update_plant_field(c.plant_data);}
  auto a=Clock::now();sim.advance(1,phase==3?15.f:0.f,false);costs[phase]+=std::chrono::duration<double>(Clock::now()-a).count();++steps[phase];
  if(sim.time()>=next){worst=std::max(worst,sim.max_containment_error());a=Clock::now();auto state=sim.snapshot();snapshot+=std::chrono::duration<double>(Clock::now()-a).count();++snapshots;auto n=sim.counts();
   std::cerr<<std::setprecision(6)<<"sim="<<sim.time()<<" wall="<<std::chrono::duration<double>(Clock::now()-begin).count()<<" WH="<<n[6]<<" RH="<<n[7]<<" MX="<<n[8]<<'\n';next+=5;}
 }
 double wall=std::chrono::duration<double>(Clock::now()-begin).count();auto n=sim.counts();
 std::cout<<std::setprecision(12)<<"{\"display_particles\":"<<p.size()<<",\"solver_particles\":"<<p.size()/8<<",\"sim_s\":"<<sim.time()<<",\"wall_s\":"<<wall<<",\"rate\":"<<sim.time()/wall<<",\"completed\":"<<(sim.time()>=duration?"true":"false")<<",\"snapshot_mean_ms\":"<<snapshot/std::max(1u,snapshots)*1000<<",\"max_containment_m\":"<<worst<<",\"phases\":[";
 for(int i=0;i<4;++i)std::cout<<(i?",":"")<<"{\"phase\":"<<i<<",\"steps\":"<<steps[i]<<",\"compute_s\":"<<costs[i]<<",\"step_mean_ms\":"<<costs[i]/std::max(1u,steps[i])*1000<<"}";
 std::cout<<"],\"zones\":[";for(int i=0;i<10;++i)std::cout<<(i?",":"")<<n[i];std::cout<<"]}\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
