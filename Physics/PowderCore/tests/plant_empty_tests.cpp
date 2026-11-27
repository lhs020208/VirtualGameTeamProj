#include "powder/plant_charge.hpp"
#include "powder/plant_empty.hpp"
#include "powder/plant_pressure.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
namespace {void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}}
int main(int argc,char** argv){try{
 check(argc==2,"data directory required");std::string root=argv[1];
 auto data=powder::read_plant_data(root+"/runtime.plant");auto routes=powder::read_plant_routes(root+"/runtime.routes");
 std::vector<powder::Grain> original;std::vector<int> suppliers;std::ifstream file(root+"/test_charge.csv");std::string line;std::getline(file,line);
 while(std::getline(file,line)){std::replace(line.begin(),line.end(),',',' ');std::istringstream in(line);int id,s;std::string supplier;powder::Grain p{};in>>id>>s>>supplier>>p.x>>p.y>>p.z;check(bool(in),"charge read");p.w=float(s);original.push_back(p);suppliers.push_back(int(supplier.back()-'A'));}
 std::vector<int> counts(6,0);auto none=powder::resize_plant_charge(data,routes,original,suppliers,counts,.002f);check(none.particles.empty(),"zero charge");
 counts={2,3,0,0,0,0};auto small=powder::resize_plant_charge(data,routes,original,suppliers,counts,.002f);check(small.particles.size()==5&&small.suppliers==std::vector<int>({0,0,1,1,1}),"independent counts");
 counts={10000,10000,100,100,100,100};auto large=powder::resize_plant_charge(data,routes,original,suppliers,counts,.002f);
 powder::GranularConfig c;c.plant=true;c.plant_data=data;c.radius=.002f;c.count=unsigned(large.particles.size());powder::validate_granular_initial(c,large.particles);check(c.count==20400,"increase and previously empty suppliers");
 bool rejected=false;counts[0]=-1;try{powder::resize_plant_charge(data,routes,original,suppliers,counts,.002f);}catch(...){rejected=true;}check(rejected,"negative count accepted");
 c.count=0;c.dt=.001f;c.plant_powered_valves=true;c.plant_powered_mixer=true;powder::GranularGPU empty(c,{},{});
 check(empty.device().find("CPU equipment only")!=std::string::npos&&empty.plant_memory_bytes()==std::array<std::uint64_t,3>{0,0,0},"zero mode allocated GPU memory");
 check(empty.snapshot().empty()&&empty.velocity_snapshot().empty()&&empty.plant_air_snapshot().empty(),"zero particle snapshots");empty.request_snapshot();std::vector<powder::Grain> output=original;check(empty.receive_snapshot(output)&&output.empty(),"empty async read");
 auto commands=data;powder::plant_set_regulated_pressures(commands,routes,{},1,63,true,true,false);int base=int(data[5].x),n=int(data[5].y);for(int i=0;i<n;++i)commands[base+2*i+1].y=1;
 empty.update_plant_controls(commands);empty.set_plant_mixer_power(0);empty.advance(100,15,false);check(empty.plant_mixer_state().stopped()&&empty.plant_mixer_state().energy_j==0,"zero power startup");
 empty.set_plant_mixer_power(1);empty.advance(100,15,false);check(empty.plant_mixer_state().rpm()>0&&empty.plant_mixer_state().rpm()<1,"instant mixer speed");
 for(int i=0;i<25;++i)empty.advance(1000,15,false);check(std::abs(empty.time()-25.2)<1.e-4,"empty clock");
 for(auto& v:empty.plant_valve_states()){check(std::abs(v.angle_rad-1.5707963268)<.001&&!v.tripped,"empty valve full travel");check(v.aperture()>=0&&v.aperture()<=1,"full-open aperture roundoff");}
 auto m=empty.plant_mixer_state();check(std::abs(m.rpm()-15)<.1&&m.energy_j>0&&m.electrical_w<=1302.01,"empty motor ratings");
 empty.advance(1,0,false);check(!empty.plant_mixer_state().stopped(),"instant braking");for(int i=0;i<3;++i)empty.advance(1000,0,false);check(empty.plant_mixer_state().stopped(),"braking failed");
 empty.set_state({},{});check(empty.time()==0&&empty.plant_mixer_state().energy_j==0,"reset failed");
 // Compare CPU empty integrator at the same dt with actual OpenCL kernels, with one distant grain.
 auto cfg=c;cfg.count=1;cfg.dt=.00025f;cfg.radius=.01f;cfg.mixer_drive.stiffness_n_m=1000;cfg.small_valve.stiffness_n_m=cfg.large_valve.stiffness_n_m=1000;
 auto g=data[base];std::vector<powder::Grain> p{{g.x,g.y,g.z+1.f,0}};powder::GranularGPU gpu(cfg,p,std::vector<powder::Grain>(1));powder::EmptyPlantDrive reference(cfg);
 gpu.update_plant_controls(commands);reference.controls(commands);for(int i=0;i<1000;++i)reference.advance(cfg.dt,15);gpu.advance(1000,15,false);
 auto actual=gpu.plant_mixer_state();check(std::abs(actual.rpm()-reference.mixer.rpm())<.01&&std::abs(actual.angle_rad-reference.mixer.angle_rad)<1.e-4,"CPU/OpenCL mixer mismatch");
 auto valves=gpu.plant_valve_states();for(size_t i=0;i<valves.size();++i){std::cout<<i<<" GPU "<<valves[i].angle_rad<<" CPU "<<reference.valves[i].angle_rad<<" contact "<<valves[i].contact_nm<<std::endl;check(std::abs(valves[i].angle_rad-reference.valves[i].angle_rad)<.001,"CPU/OpenCL valve mismatch");}
 std::cout<<"PASS independent counts, 20400 nonoverlapping generated grains, zero GPU allocation, clock, valves, powered mixer and CPU/OpenCL equivalence\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
