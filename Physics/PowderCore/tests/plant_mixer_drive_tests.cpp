#include "powder/plant.hpp"
#include <iostream>
#include <array>
#include <algorithm>
namespace {void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}}
int main(int argc,char**argv){try{
 check(argc==2,"data directory required");auto data=powder::read_plant_data(std::string(argv[1])+"/runtime.plant");
 powder::GranularConfig c;c.plant=true;c.plant_data=data;c.plant_threaded=true;c.plant_wall_mode=5;c.plant_powered_mixer=true;c.radius=.01f;c.dt=.00025f;c.count=1;c.iterations=4;c.mixer_drive.stiffness_n_m=1000;
 auto gate=data[int(data[5].x)];std::vector<powder::Grain> p{{gate.x,gate.y,gate.z+.013f,0}},zero(1);
 auto seconds=[](powder::GranularGPU& gpu,int batches,float rpm){for(int i=0;i<batches;++i)gpu.advance(1000,rpm,false);};
 {powder::GranularGPU gpu(c,p,zero);gpu.set_plant_mixer_power(0);gpu.advance(100,15,false);auto s=gpu.plant_mixer_state();check(s.stopped()&&s.energy_j==0,"unpowered start");
  gpu.set_plant_mixer_power(1);gpu.advance(100,15,false);s=gpu.plant_mixer_state();check(s.rpm()>0&&s.rpm()<1,"start teleports to target rpm");
  seconds(gpu,12,15);s=gpu.plant_mixer_state();std::cout<<"unloaded rpm="<<s.rpm()<<" angle="<<s.angle_rad<<" energy_J="<<s.energy_j<<std::endl;
  check(std::abs(s.rpm()-15)<.1&&!s.tripped,"unloaded target");check(std::abs(gpu.angle()-s.angle_rad)<1e-6,"render angle differs from physical shaft");check(s.electrical_w<=c.mixer_drive.input_w&&s.energy_j>0,"input power budget");
  gpu.advance(1,0,false);check(!gpu.plant_mixer_state().stopped(),"stop teleports shaft");seconds(gpu,12,0);check(gpu.plant_mixer_state().stopped(),"finite braking did not stop");
  seconds(gpu,10,15);gpu.set_plant_mixer_power(0);gpu.advance(1,15,false);auto cut=gpu.plant_mixer_state();check(!cut.stopped()&&cut.electrical_w==0,"power cut must coast with no electricity");seconds(gpu,6,15);s=gpu.plant_mixer_state();check(s.stopped()&&s.energy_j==cut.energy_j,"power-off braking/energy");
  gpu.set_state(p,zero);check(gpu.plant_mixer_state().stopped()&&gpu.plant_mixer_state().angle_rad==0&&gpu.plant_mixer_state().energy_j==0,"reset drive state");
 }
 {auto loaded=c;loaded.mixer_drive.test_resisting_nm=500;powder::GranularGPU gpu(loaded,p,zero);seconds(gpu,14,15);auto s=gpu.plant_mixer_state();std::cout<<"loaded rpm="<<s.rpm()<<" torque="<<s.motor_nm<<" watts="<<s.electrical_w<<std::endl;check(s.rpm()>0&&s.rpm()<14.5&&!s.tripped,"load did not reduce speed");check(std::abs(s.motor_nm)<=750.001&&s.electrical_w<=1302.01,"rated limits");}
 {auto jam=c;jam.mixer_drive.test_resisting_nm=1000;powder::GranularGPU gpu(jam,p,zero);seconds(gpu,12,15);auto s=gpu.plant_mixer_state();std::cout<<"jam rpm="<<s.rpm()<<" tripped="<<s.tripped<<std::endl;check(s.stopped()&&s.tripped&&s.electrical_w==0,"overload trip");gpu.advance(1,15,false);check(gpu.plant_mixer_state().tripped,"same command bypasses trip");gpu.advance(1,0,false);check(!gpu.plant_mixer_state().tripped,"stop does not acknowledge fault");}
 // Real revision-1125 rotating mesh, production sphere/dt. Select a face with
 // a negative X lever arm; compare the exact force ledger with shaft reaction.
 {auto contact=c;contact.radius=.002f;contact.dt=.00001f;contact.mixer_drive.stiffness_n_m=5000;contact.mixer_drive.inertia_kg_m2=.1f;contact.mixer_drive.torque_nm=.001f;contact.mixer_drive.brake_nm=0;contact.mixer_drive.bearing_nm_s=0;
  bool tested=false;auto pivot=data[1];
  for(int node=int(data[12].y);node<int(data[13].y)&&!tested;node+=2){auto lo=data[node],hi=data[node+1];if(lo.w>=0)continue;
   for(int j=0;j<int(-lo.w)&&!tested;++j){auto ref=data[int(hi.w)+j];if(ref.y!=1)continue;int k=int(ref.x);auto a=data[k],b=data[k+1],d=data[k+2];
    std::array<double,3> ab{b.x-a.x,b.y-a.y,b.z-a.z},ac{d.x-a.x,d.y-a.y,d.z-a.z};std::array<double,3> normal{ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0]};double len=std::hypot(normal[0],normal[1],normal[2]);if(len<1e-8)continue;for(auto& x:normal)x/=len;
    powder::Grain q{float((a.x+b.x+d.x)/3.+normal[0]*.0018),float((a.y+b.y+d.y)/3.+normal[1]*.0018),float((a.z+b.z+d.z)/3.+normal[2]*.0018),0};
    if((q.y-pivot.y)*normal[2]-(q.z-pivot.z)*normal[1]<.08)continue;
    powder::GranularGPU gpu(contact,{q},zero);gpu.advance(1,15,false);auto s=gpu.plant_mixer_state();auto ledger=gpu.plant_mixer_contact_snapshot();auto acc=ledger[0];double mass=4.188790204786391*std::pow(contact.radius,3)*data[1].w;
    double expected=-mass*((q.y-pivot.y)*acc.z-(q.z-pivot.z)*acc.y);
    check(s.normal_n>0&&s.contact_nm<0,"real paddle contact absent");check(std::abs(s.contact_nm-expected)<1e-5,"contact reaction violates action/reaction");check(s.omega_rad_s<0,"particle load not coupled to rotor inertia");
    std::cout<<"real paddle contact_Nm="<<s.contact_nm<<" expected="<<expected<<" rpm="<<s.rpm()<<std::endl;
    auto production=c;production.radius=.002f;production.dt=.00001f;production.plant_wall_mode=3;production.plant_powered_valves=true;production.mixer_drive.stiffness_n_m=5000;
    powder::GranularGPU stable(production,{q},zero);stable.advance(1,15,false);check(stable.plant_mixer_state().normal_n>0,"production cached hybrid contact absent");
    stable.advance(1000,15,false);stable.advance(1000,15,false);auto moving=stable.plant_mixer_state();auto point=stable.snapshot()[0];
    check(std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z)&&moving.rpm()>0&&!moving.tripped,"production contact integration unstable");
    std::cout<<"production mesh contact integration_s="<<stable.time()<<" rpm="<<moving.rpm()<<" powered_valves="<<stable.plant_valve_states().size()<<std::endl;tested=true;
   }
  }check(tested,"no suitable actual rotor face found");
 }
 std::cout<<"PASS powered mixer: ramp, load, stall, electricity, braking, reset, actual mesh reaction"<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
