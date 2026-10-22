#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>
namespace powder {
// Restricted straight-channel finite-volume prototype. SI units; no individual contacts.
class BulkPrototype {
public:
 using Mass=std::array<double,3>;
 struct Marker {double x=0,y=0,z=0,vz=0;unsigned group=0;};
 struct Config {unsigned cells=128,markers=1500;double mass=30,length=5,area=.16,bulk_density=800,tau=.1,gravity=3.355217606,normal_gravity=-9.21838461;};
 const Config config;std::vector<Mass> cells;std::vector<Marker> markers;
 double time=0,velocity=0;double air=1;bool valve_open=true;unsigned valve=0;
 explicit BulkPrototype(Config c):config(c){
  need(c.cells>=20&&c.cells<=2048&&c.markers<=100000,"invalid resolution");
  for(double x:{c.mass,c.length,c.area,c.bulk_density,c.tau})need(std::isfinite(x)&&x>0,"invalid physical setting");need(std::isfinite(c.gravity)&&std::isfinite(c.normal_gravity),"invalid gravity");
  cells.resize(c.cells);valve=c.cells*2/5;unsigned charged=c.cells/5;
  need(c.mass/charged<=capacity(),"initial charge exceeds bulk capacity");
  for(unsigned i=0;i<charged;++i)cells[i][i%3]=c.mass/charged;
  markers.resize(c.markers);for(unsigned i=0;i<c.markers;++i){auto& p=markers[i];unsigned cell=i%charged;p.x=std::clamp((cell+.1+.8*double((i*37)%997)/997)*dx(),.02,c.length-.02);p.y=(double((i*67)%991)/991-.5)*.32;p.z=(double((i*97)%983)/983-.5)*.32;p.group=cell%3;}
 }
 double dx()const{return config.length/config.cells;}
 double capacity()const{return dx()*config.area*config.bulk_density;}
 static double total(const Mass& m){return m[0]+m[1]+m[2];}
 Mass masses()const{Mass result{};for(auto m:cells)for(unsigned g=0;g<3;++g)result[g]+=m[g];return result;}
 static void need(bool b,const char* message){if(!b)throw std::invalid_argument(message);}
 void advance(double duration){
  need(std::isfinite(duration)&&duration>0&&duration<=1,"invalid duration");need(std::isfinite(air)&&std::abs(air)<=5,"air outside prototype range");
  double remaining=duration;while(remaining>0){double bound=std::max(std::abs(velocity),std::abs(air+config.gravity*config.tau));double h=std::min(remaining,.4*dx()/std::max(bound,1e-9));substep(h);remaining=std::max(0.,remaining-h);}time+=duration;
 }
private:
 void substep(double h){
  const double target=air+config.gravity*config.tau;velocity=target+(velocity-target)*std::exp(-h/config.tau);
  // Uniform projected drag/gravity velocity; valve and capacity restrict mass flux separately.
  auto next=cells;std::vector<double> face(config.cells+1,0);
  for(unsigned f=1;f<config.cells;++f){if(f==valve&&!valve_open)continue;unsigned from=velocity>=0?f-1:f,to=velocity>=0?f:f-1;
   double mass=total(cells[from]);if(mass<=0)continue;
   double amount=std::min({mass,std::abs(velocity)*h/dx()*mass,std::max(0.,capacity()-total(cells[to]))});
   face[f]=(velocity>=0?1:-1)*amount*dx()/(mass*h);
   for(unsigned g=0;g<3;++g){double moved=amount*(cells[from][g]/mass);next[from][g]-=moved;next[to][g]+=moved;}
  }
  // Tracers carry no mass. Velocity is constrained by the same flux and closed boundaries.
  for(auto& p:markers){unsigned i=std::min(config.cells-1,static_cast<unsigned>(p.x/dx()));double fraction=std::clamp(p.x/dx()-i,0.,1.);double u=face[i]*(1-fraction)+face[i+1]*fraction;double x=std::clamp(p.x+u*h,.02,config.length-.02);
   if(!valve_open){double gate=valve*dx();if(p.x<gate)x=std::min(x,gate-.03);else x=std::max(x,gate+.03);}p.x=x;double terminal=config.normal_gravity*config.tau;p.vz=terminal+(p.vz-terminal)*std::exp(-h/config.tau);p.z+=p.vz*h;if(p.z<-.18){p.z=-.18;p.vz=std::max(0.,p.vz);}if(p.z>.18){p.z= .18;p.vz=std::min(0.,p.vz);}
  }
  cells.swap(next);
 }
};
}
