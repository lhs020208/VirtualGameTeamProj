#include "powder/plant_pressure.hpp"
#include <iostream>
#include <iomanip>
namespace {void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}}
int main(int argc,char**argv){try{
 check(argc==2,"data directory required");auto data=powder::read_plant_data(std::string(argv[1])+"/runtime.plant");auto routes=powder::read_plant_routes(std::string(argv[1])+"/runtime.routes");
 auto gates=powder::plant_supplier_gates(data);auto a=data[gates[0]],b=data[gates[1]];
 powder::plant_set_regulated_pressures(data,routes,{},1,1,false,false,false);
 powder::GranularConfig c;c.plant=true;c.plant_powered_valves=true;c.plant_threaded=true;c.plant_wall_mode=5;c.plant_data=data;c.radius=.01f;c.dt=.00025f;c.count=1;c.iterations=4;
 // Coarse mechanical travel test: larger spheres and lower contact stiffness allow dt=0.25 ms.
 // Production uses 2 mm, 5000 N/m and dt=10 us; its GPU contact check follows below.
 c.small_valve.stiffness_n_m=c.large_valve.stiffness_n_m=1000;
 std::vector<powder::Grain> seed{{b.x,b.y,b.z+.003f+c.radius,0}},zero(1);
 {powder::GranularGPU gpu(c,seed,zero);gpu.set_plant_valve_power(0,0);gpu.advance(100,0,false);auto v=gpu.plant_valve_states()[0];check(v.closed()&&v.energy_j==0,"unpowered actuator moved/used electricity");
  gpu.set_plant_valve_power(0,1);double start=gpu.time();for(int i=0;i<60&&!gpu.plant_valve_states()[0].open();++i)gpu.advance(1000,0,false);
  v=gpu.plant_valve_states()[0];double elapsed=gpu.time()-start;std::cout<<"unloaded travel_s="<<elapsed<<" angle="<<v.angle_rad<<" energy_J="<<v.energy_j<<std::endl;
  check(v.open()&&elapsed>=10&&elapsed<=13,"unloaded catalog travel or limit switch");check(v.energy_j>0&&v.energy_j<=15*elapsed,"electrical budget");
  // Closing is a finite reverse drive; requests do not replace the current shaft state.
  powder::plant_set_regulated_pressures(data,routes,{},0,0,false,false,false);gpu.update_plant_controls(data);check(gpu.plant_valve_states()[0].open(),"command teleported valve");gpu.advance(100,0,false);check(gpu.plant_valve_states()[0].angle_rad<v.angle_rad,"closing did not reverse");
  gpu.set_plant_valve_power(0,0);gpu.advance(100,0,false);auto held=gpu.plant_valve_states()[0];gpu.advance(100,0,false);v=gpu.plant_valve_states()[0];check(std::abs(held.angle_rad-v.angle_rad)<1e-6&&v.energy_j==held.energy_j,"power-cut holding/energy");
 }
 {auto jam=c;jam.small_valve.external_torque_nm=-20;powder::GranularGPU gpu(jam,seed,zero);for(int i=0;i<10;++i)gpu.advance(1000,0,false);auto v=gpu.plant_valve_states()[0];
  std::cout<<"external jam angle="<<v.angle_rad<<" tripped="<<v.tripped<<std::endl;check(v.tripped&&v.closed(),"finite motor did not stall/trip");check(std::abs(v.motor_nm)<=10.001,"motor exceeds rated torque");}
 // Two actual particles press opposite faces on opposite arms: reactions must ADD, not cancel.
 {auto pin=c;pin.count=2;pin.small_valve.torque_nm=.001f;pin.small_valve.holding_nm=.001f;pin.small_valve.inertia_kg_m2=.1f;
  std::vector<powder::Grain> p{{a.x,a.y+.012f,a.z+.012f,0},{a.x,a.y-.012f,a.z-.012f,1}};
  powder::GranularGPU gpu(pin,p,std::vector<powder::Grain>(2));gpu.advance(1,0,false);auto v=gpu.plant_valve_states()[0];
  std::cout<<"two_face contact_Nm="<<v.contact_nm<<" normal_N="<<v.normal_n<<" angle="<<v.angle_rad<<std::endl;
  check(v.contact_nm<-.02&&v.normal_n>1.9&&v.closed(),"two-sided particle reaction not coupled to shaft");
  auto vel=gpu.velocity_snapshot();check(vel[0].z>0&&vel[1].z<0,"equal/opposite particle forces absent");
 }
 {auto production=c;production.radius=.002f;production.dt=.00001f;production.count=2;production.small_valve.stiffness_n_m=5000;production.small_valve.torque_nm=.001f;production.small_valve.holding_nm=.001f;
  std::vector<powder::Grain> p{{a.x,a.y+.012f,a.z+.0048f,0},{a.x,a.y-.012f,a.z-.0048f,1}};
  powder::GranularGPU gpu(production,p,std::vector<powder::Grain>(2));gpu.advance(1,0,false);auto v=gpu.plant_valve_states()[0];check(v.contact_nm<-.02&&v.closed(),"production dt/radius pinching");
  std::cout<<"production contact_Nm="<<v.contact_nm<<" normal_N="<<v.normal_n<<std::endl;
 }
 std::vector<double> openings(gates.size()+2,0);auto p=powder::PlantPressureTargets{500,-500,-1000};double previous=-1;
 for(double fraction:{0.,.05,.25,.5,1.}){openings[0]=fraction;powder::plant_set_regulated_pressures(data,routes,p,0,0,false,false,false,{},&openings);double q=data[routes[0].cells[0]+2].x;check(q>=previous&&((fraction==0)==(q==0)),"actual aperture flow monotonicity");previous=q;}
 std::cout<<"PASS powered valves: finite travel, electricity, reverse, torque stall, bilateral reaction, actual aperture"<<std::endl;
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
