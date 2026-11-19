#include "powder/plant.hpp"
#include "powder/plant_feed_brake.hpp"
#include <iostream>
#include <cmath>
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static double delta(powder::Grain a,powder::Grain b){return std::hypot(std::hypot(double(a.x)-b.x,double(a.y)-b.y),double(a.z)-b.z);}
int main(int argc,char**argv){try{
 require(argc==2,"runtime directory required");std::string dir=argv[1];auto data=powder::read_plant_data(dir+"/runtime.plant");auto routes=powder::read_plant_routes(dir+"/runtime.routes");
 powder::plant_set_manual_valves(data,routes,1000,0,0,false,false);powder::plant_set_receiver_valve(data,false);
 std::vector<powder::Grain> points,vel;std::array<powder::PlantFeedBrake,3> brakes;
 for(unsigned i=0;i<3;++i){brakes[i]=powder::plant_feed_brake(data,i,.002f);auto r=brakes[i].region;points.push_back({r.x,r.y,r.z+.1f,float(i)});vel.push_back({0,0,-.4f,0});}
 powder::GranularConfig c;c.plant=true;c.plant_threaded=true;c.kernel_optimize=true;c.plant_wall_mode=5;c.plant_data=data;c.radius=.002f;c.dt=.00001f;c.iterations=8;c.count=3;
 powder::GranularGPU cl(c,points,vel);c.backend=powder::GranularBackend::CudaPtx;powder::GranularGPU cu(c,points,vel);c.backend=powder::GranularBackend::CudaPtxGraph;powder::GranularGPU graph(c,points,vel);
 for(int phase=0;phase<5;++phase){
  for(unsigned i=0;i<3;++i){brakes[i].enabled=phase==1||phase==3;brakes[i].response_time_s=phase==3?.002f:.001f;cl.set_plant_feed_brake(brakes[i],i);cu.set_plant_feed_brake(brakes[i],i);graph.set_plant_feed_brake(brakes[i],i);}
  auto before=cu.snapshot();auto reference=cl.snapshot();for(size_t i=0;i<3;++i)require(delta(before[i],reference[i])<2e-6,"brake setter changed positions");
  cl.advance(16,0,false);cu.advance(16,0,false);graph.advance(16,0,false);
  auto a=cl.snapshot(),b=cu.snapshot(),g=graph.snapshot(),av=cl.velocity_snapshot(),bv=cu.velocity_snapshot(),gv=graph.velocity_snapshot();
  auto air=cl.plant_air_snapshot(),cair=cu.plant_air_snapshot(),gair=graph.plant_air_snapshot();
  for(size_t i=0;i<3;++i){require(delta(a[i],b[i])<2e-6&&delta(b[i],g[i])<2e-6,"position equivalence");require(delta(av[i],bv[i])<.002&&delta(bv[i],gv[i])<.002,"velocity equivalence");require(delta(air[i],cair[i])<1e-5&&delta(cair[i],gair[i])<1e-5,"air equivalence");}
 }
 for(auto* gpu:{&cl,&cu,&graph}){gpu->set_state(points,vel);gpu->advance(16,0,false);require(gpu->velocity_snapshot()[0].z<-.4f,"reset must release brakes");}
 std::cout<<"PASS CUDA UI: three brakes, toggle/reconfigure graph invalidation, snapshots, air inspection, reset\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
