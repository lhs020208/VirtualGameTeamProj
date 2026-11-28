#include "powder/plant_pressure.hpp"
#include <iostream>
#include <cstring>
#include <limits>
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("data directory");std::string root=argv[1];auto d=powder::read_plant_data(root+"/runtime.plant"),original=d;auto routes=powder::read_plant_routes(root+"/runtime.routes");
 unsigned cases=0;
 for(auto p:{powder::PlantPressureTargets{NAN,0,0},{0,INFINITY,0},{1501,0,0},{0,-1501,0},{0,0,1},{0,0,-1501}}){bool rejected=false;try{powder::plant_set_regulated_pressures(d,routes,p,3,3,true,true,false);}catch(const std::invalid_argument&){rejected=true;}if(!rejected||std::memcmp(d.data(),original.data(),d.size()*sizeof(d[0])))throw std::runtime_error("invalid input changed state");++cases;}
 for(int stage=0;stage<6;++stage)for(unsigned mask:{0u,1u,2u,3u,63u})for(bool wh:{false,true})for(bool rh:{false,true}){
  auto p=powder::plant_pressure_preset(stage);auto state=powder::plant_set_regulated_pressures(d,routes,p,mask,mask,wh,wh,rh);double in=0,out=0;unsigned supplier=0;
  if(state.applied.receiver!=(rh?0:p.receiver))throw std::runtime_error("receiver vent override");
  for(const auto&r:routes){double q=d[r.cells[0]+2].x,dp=r.phase==0?p.supply-p.weighing:p.weighing-state.applied.receiver;bool open=r.phase==0?(mask&(1u<<supplier++))!=0:wh;
   if((!open||dp==0)?q!=0:q*dp<=0)throw std::runtime_error("flow direction / closed valve");
   for(int at:r.cells)if(d[at+2].x!=q)throw std::runtime_error("route continuity");if(r.phase==0)in+=q;else out=q;
  }
  if(std::abs(in+state.weighing_regulator_m3s-out)>1e-7)throw std::runtime_error("regulated reservoir balance");++cases;
 }
 // Pending closure cuts the carrier flow before the occupied physical disk closes.
 auto pending=powder::plant_set_regulated_pressures(d,routes,{500,-500,-1000},3,0,true,false,false);for(const auto&r:routes)if(d[r.cells.front()+2].x!=0)throw std::runtime_error("pending closure flow");
 for(auto p:{powder::PlantPressureTargets{1000,-1000,-1000},{-1000,1000,0}}){powder::plant_set_regulated_pressures(d,routes,p,63,63,true,true,false);powder::validate_plant_data(d);++cases;}
 powder::GranularConfig c;c.plant=true;c.plant_threaded=true;c.kernel_optimize=true;c.plant_wall_mode=5;c.radius=.002f;c.dt=.00001f;c.iterations=8;c.count=1;c.plant_data=d;
 std::vector<powder::Grain> seed{{2.3f,0,2.682f,0}},zero(1);powder::GranularGPU gpu(c,seed,zero);
 double vx[3]{};for(int i=0;i<3;++i){powder::PlantPressureTargets p{0,i==0?500.:i==1?-500.:0.,0};powder::plant_set_regulated_pressures(d,routes,p,0,0,true,true,false);gpu.update_plant_field(d);gpu.set_state(seed,zero);gpu.advance(16,0,false);vx[i]=gpu.velocity_snapshot()[0].x;}
 if(!(vx[0]>0&&vx[1]<0&&std::abs(vx[2])<1e-7))throw std::runtime_error("pressure did not drive signed GPU drag");
 for(int i=0;i<30;++i){auto p=powder::plant_pressure_preset(i%6);powder::plant_set_regulated_pressures(d,routes,p,3,3,i%2!=0,i%2!=0,i%3==0);gpu.update_plant_field(d);gpu.set_state(seed,zero);gpu.advance(16,0,false);auto a=gpu.snapshot()[0];if(!std::isfinite(a.x+a.y+a.z))throw std::runtime_error("repeated GPU update");}
 std::cout<<"PASS CPU cases="<<cases<<"; invalid atomic rejection, presets, signed flow, closed/pending valves, receiver vent, regulator balance; GPU vx="<<vx[0]<<","<<vx[1]<<","<<vx[2]<<"; 30 repeated updates\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
