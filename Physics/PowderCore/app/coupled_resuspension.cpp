#include "powder/coupled.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <chrono>
using namespace powder;
int main(int argc,char** argv){try{
 if(argc<2||argc>4)throw std::runtime_error("output [dt] [gamma] required");std::filesystem::path out(argv[1]);if(std::filesystem::exists(out))throw std::runtime_error("output exists");std::filesystem::create_directories(out);
 GasConfig c;c.cells={4,4,8};c.h=.005;c.boundary_pa={0,0,0,0,101325,101325};CoupledWorld w(c);auto base=coupled_demo(1,0);w.dem=base.dem;w.dem.bodies.clear();w.dem.planes={{1,{0,0,1},0},{2,{0,0,-1},-.01},{3,{1,0,0},0},{4,{-1,0,0},-.02},{5,{0,1,0},0},{6,{0,-1,0},-.02}};
 auto b=make_clump(1,0,{{{},.001}},1000);double mass=clump_properties(b.pebbles,1000).mass_kg,r=.0005,gamma=argc>3?std::stod(argv[3]):.002,pull=4*3.141592653589793*gamma*r;
 auto& m=w.dem.wall_models[0];m.adhesion_gamma_j_m2=gamma;m.linear.cn=.003;m.linear.ct=.0001;b.position_m={.01,.01,r-(mass*9.81+pull)/m.linear.kn};w.dem.bodies.push_back(b);w.initialize();
 std::ofstream trace(out/"trace.csv");trace<<std::setprecision(17)<<"t,z,vz,gas_vz,drop,gas_balance_kg,particle_balance_kg\n";
 bool lifted=false,hit=false;double maxz=b.position_m.z,maxv=0,minz=maxz,heldStart=-1;auto start=std::chrono::steady_clock::now();
 const double dt=argc>2?std::stod(argv[2]):2e-7;if(!std::isfinite(dt)||dt<=0||dt>2e-7)throw std::runtime_error("scenario dt outside (0,2e-7]");for(int i=0;i<int(std::ceil(.3/dt));++i){double t=w.dem.physical_time_s;double drop=t<.005?0:t<.06?2000:t<.16?2000*(.16-t)/.1:0;w.gas.config.boundary_pa[5]=101325-drop;auto a=w.step(dt);auto& body=w.dem.bodies.at(0);maxz=std::max(maxz,body.position_m.z);minz=std::min(minz,body.position_m.z);maxv=std::max(maxv,std::abs(body.velocity_m_s.z));if(body.position_m.z>r+1e-5)lifted=true;if(body.position_m.z>=.01-r)hit=true;if(t>=.20&&body.position_m.z>=.01-r-1e-6){if(heldStart<0)heldStart=t;}else if(t>=.20)heldStart=-1;
 if(i%1000==0)trace<<a.time<<','<<body.position_m.z<<','<<body.velocity_m_s.z<<','<<w.gas.velocity(w.gas.index(2,2,1)).z<<','<<drop<<','<<a.gas_balance_error<<','<<a.particle_balance_error<<'\n';
 }
 const auto a=w.report();double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();bool held=heldStart>=0&&a.time-heldStart>=.08;
 std::ofstream result(out/"result.json");result<<std::setprecision(17)<<"{\"lifted\":"<<lifted<<",\"filter_contact\":"<<hit<<",\"retained_after_air_off\":"<<held<<",\"max_z\":"<<maxz<<",\"min_z\":"<<minz<<",\"max_speed\":"<<maxv<<",\"final_z\":"<<w.dem.bodies[0].position_m.z<<",\"time_s\":"<<a.time<<",\"wall_s\":"<<wall<<",\"gas_balance_kg\":"<<a.gas_balance_error<<",\"particle_balance_kg\":"<<a.particle_balance_error<<"}";
 std::cout<<"lift="<<lifted<<" hit="<<hit<<" retained="<<held<<" maxz="<<maxz<<" final="<<w.dem.bodies[0].position_m.z<<" rate="<<a.time/wall<<'\n';return lifted&&hit&&(gamma==0?!held:held)?0:2;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
