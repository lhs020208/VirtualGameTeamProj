#include "powder/coupled.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace powder;
static void require(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
template<class F> void reject(F f){bool threw=false;try{f();}catch(const std::invalid_argument&){threw=true;}require(threw,"invalid port accepted");}
int main(){try{
 GasConfig c;c.cells={4,4,4};c.boundary_pa={};GasGrid closed(c);
 for(int z=0;z<4;++z)for(int y=0;y<4;++y)closed.cells[closed.index(0,y,z)].solid=true;
 closed.initialize_pressure();int cell=closed.index(1,1,1);
 auto equal=closed;equal.pressure_ports.push_back({cell,0,0,101325});
 for(int k=0;k<20;++k)equal.step(1e-6);
 require(std::abs(equal.boundary_mass_out)<1e-15,"equal pressure created mass flux");
 require(std::abs(equal.velocity(cell).x)<1e-12,"equal pressure created motion");
 auto inlet=closed;inlet.pressure_ports.push_back({cell,0,0,101425});
 double mass=inlet.mass();auto grad=inlet.pressure_gradient(cell);
 require(std::abs(grad.x+100/c.h)<1e-8,"port pressure gradient inconsistent");
 for(int k=0;k<50;++k)inlet.step(1e-6);
 require(inlet.mass()>mass&&inlet.boundary_mass_out<0,"pressure inlet did not admit gas");
 require(inlet.velocity(cell).x>0,"pressure drove flow in wrong direction");
 require(std::abs(inlet.mass()+inlet.boundary_mass_out-mass)<1e-14,"interior port mass ledger failed");
 auto outlet=closed;outlet.pressure_ports.push_back({cell,0,0,101225});
 for(int k=0;k<50;++k)outlet.step(1e-6);
 require(outlet.mass()<mass&&outlet.boundary_mass_out>0,"pressure outlet did not remove gas");
 require(outlet.velocity(cell).x<0,"outlet flow sign incorrect");
 auto bad=closed;bad.pressure_ports={{cell,0,1,101325}};reject([&]{bad.step(1e-6);});
 require(bad.mass()==mass&&bad.boundary_mass_out==0,"invalid port changed state");
 bad.pressure_ports={{cell,0,0,101325},{cell,0,0,101325}};reject([&]{bad.step(1e-6);});
 bad.pressure_ports={{cell,0,0,std::numeric_limits<double>::quiet_NaN()}};reject([&]{bad.step(1e-6);});
 bad.pressure_ports={{closed.index(0,1,1),0,0,101325}};reject([&]{bad.step(1e-6);});
 auto old_boundary=GasGrid(c);old_boundary.config.boundary_pa[0]=101425;
 auto face_ports=GasGrid(c);for(int z=0;z<4;++z)for(int y=0;y<4;++y)face_ports.pressure_ports.push_back({face_ports.index(0,y,z),0,0,101425});
 for(int k=0;k<20;++k){old_boundary.step(1e-6);face_ports.step(1e-6);}
 for(std::size_t i=0;i<old_boundary.cells.size();++i){require(old_boundary.cells[i].mass==face_ports.cells[i].mass,"whole-face regression mass");require(old_boundary.cells[i].momentum.x==face_ports.cells[i].momentum.x,"whole-face regression momentum");}
 require(old_boundary.boundary_mass_out==face_ports.boundary_mass_out,"whole-face regression ledger");
 for(int axis=0;axis<3;++axis)for(int side=0;side<2;++side){
  GasGrid directional(c);int q[3]{1,1,1};q[axis]=side?3:0;int at=directional.index(q[0],q[1],q[2]);
  directional.pressure_ports={{at,axis,side,101425}};double initial=directional.mass();directional.step(1e-6);
  auto velocity=directional.velocity(at);double u=axis==0?velocity.x:axis==1?velocity.y:velocity.z;
  require((side?-u:u)>0,"pressure-face orientation failed");
  require(std::abs(directional.mass()+directional.boundary_mass_out-initial)<1e-14,"directional mass balance failed");
 }
 std::cout<<"gas pressure ports: equilibrium, inlet/outlet, conservation, gradient, rejection, legacy equivalence passed\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
