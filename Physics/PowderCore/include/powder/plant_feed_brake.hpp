#pragma once
#include "plant.hpp"
namespace powder {
// Numerical equivalent of an upstream braked metering feeder, not existing plant hardware.
// Geometry comes from the active supplier gate and nearest supplier separation. Two particle
// radii separate the support region from the ORIGINAL unchanged disk closure exclusion zone.
inline PlantFeedBrake plant_feed_brake(const std::vector<Grain>& d,unsigned supplier,float r,float response_time_s=.001f){
 auto gates=plant_supplier_gates(d);if(supplier>=gates.size()||!(r>0))throw std::invalid_argument("invalid brake supplier");
 auto gate=d[gates[supplier]];double separation=1.e30;
 for(unsigned j=0;j<gates.size();++j)if(j!=supplier)separation=std::min(separation,std::hypot(double(d[gates[j]].x)-gate.x,double(d[gates[j]].y)-gate.y));
 if(!(separation>4*r&&separation<100))throw std::invalid_argument("brake needs separated supplier geometry");
 PlantFeedBrake out;out.region={gate.x,gate.y,float(gate.z+.003+r+.00002+2*r),float(separation*.5-r)};out.response_time_s=response_time_s;return out;
}
inline bool plant_feed_brake_contains(Grain p,const PlantFeedBrake& b){return p.z>=b.region.z&&std::hypot(double(p.x)-b.region.x,double(p.y)-b.region.y)<=b.region.w;}
inline bool plant_feed_brake_spill_candidate(Grain p,Grain v,Grain gate,const PlantFeedBrake& b){
 // Maximum free stopping distance for dv/dt=-v/tau. Count IDs already below the
 // support face plus those that can cross it while braking; never a fixed gram offset.
 return p.z>=gate.z&&std::hypot(double(p.x)-gate.x,double(p.y)-gate.y)<=b.region.w&&
     p.z<b.region.z+std::max(0.,-double(v.z))*b.response_time_s;
}
inline unsigned plant_closure_occupancy(Grain gate,const std::vector<Grain>& particles,float radius){
 unsigned count=0;for(auto p:particles){double radial=std::hypot(p.x-gate.x,p.y-gate.y)-gate.w,axial=std::abs(p.z-gate.z)-.003;
 double distance=std::hypot(std::max(radial,0.),std::max(axial,0.))+std::min(std::max(radial,axial),0.);
 if(distance<radius+.00002)++count;}return count;
}
}
