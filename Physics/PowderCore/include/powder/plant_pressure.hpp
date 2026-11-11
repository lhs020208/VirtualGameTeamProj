#pragma once
#include "plant.hpp"
#include <array>
namespace powder {
// Ideal regulated reservoirs, gauge Pa relative to 101325 Pa. Test limits only.
struct PlantPressureTargets {double supply=0,weighing=0,receiver=0;};
inline void validate_plant_pressure_targets(PlantPressureTargets p){
 if(!std::isfinite(p.supply)||!std::isfinite(p.weighing)||!std::isfinite(p.receiver)||p.supply < -1000||p.supply>1000||p.weighing < -1000||p.weighing>1000||p.receiver < -1000||p.receiver>0)
  throw std::invalid_argument("test gauge pressure outside supply/WH [-1000,1000], receiver [-1000,0] Pa");
}
inline PlantPressureTargets plant_pressure_preset(int stage){
 switch(stage){case 0:case 2:case 4:case 5:return {};case 1:return {500,-500,0};case 3:return {0,500,-1000};default:throw std::invalid_argument("invalid pressure stage");}
}
struct PlantPressureState {PlantPressureTargets applied;double weighing_regulator_m3s=0,transfer_m3s=0;};
// The ideal WH regulator supplies q_out - sum(q_in); negative means extraction.
// This explicitly accounts for a reservoir source/sink, not a closed-node imbalance.
inline PlantPressureState plant_set_regulated_pressures(std::vector<Grain>&d,const std::vector<PlantRoute>&routes,PlantPressureTargets target,std::uint32_t suppliers,std::uint32_t supplier_air,bool hopper,bool hopper_air,bool receiver_open){
 validate_plant_pressure_targets(target);
 if(d.size()<16||d[11].x!=0)throw std::invalid_argument("regulated pressure requires full plant");
 auto gates=plant_supplier_gates(d);int wh=plant_weighing_gate(d),rh=plant_receiver_gate(d);
 PlantPressureState state;state.applied=target;if(receiver_open)state.applied.receiver=0; // vented discharge test assumption
 std::vector<double> flows;size_t branch=0,outputs=0;double input=0;
 for(const auto&r:routes){
  if(r.cells.empty()||(r.phase!=0&&r.phase!=1))throw std::invalid_argument("invalid regulated route");
  bool enabled;double dp;
  if(r.phase==0){if(branch>=gates.size())throw std::invalid_argument("too many supplier routes");enabled=(suppliers&supplier_air&(std::uint32_t(1)<<branch))!=0;++branch;dp=target.supply-target.weighing;}
  else{++outputs;enabled=hopper&&hopper_air;dp=target.weighing-state.applied.receiver;}
  auto loss=[&](double q){double sum=0;for(int at:r.cells){if(at<int(d[0].z)||at+2>=int(d.size())||(at-int(d[0].z))%3||d[at+2].w!=r.phase)throw std::invalid_argument("regulated route cell mismatch");auto a=d[at],b=d[at+1],v=d[at+2];double length=std::hypot(std::hypot(b.x-a.x,b.y-a.y),b.z-a.z);sum+=v.z==1?plant_cell_loss(q,length,2*a.w*b.w/(a.w+b.w),v.y,4*a.w*b.w):plant_cell_loss(q,length,std::min(a.w,b.w),v.y);}return sum;};
  loss(0);double q=0;
  if(enabled&&dp!=0){double lo=0,hi=1;if(loss(hi)<std::abs(dp))throw std::invalid_argument("pressure flow bracket exceeded");for(int n=0;n<55;++n){double mid=(lo+hi)/2;if(loss(mid)>std::abs(dp))hi=mid;else lo=mid;}q=std::copysign((lo+hi)/2,dp);}
  for(int at:r.cells){double radius=std::min(d[at].w,d[at+1].w);if(std::abs(q)/(3.141592653589793*radius*radius)>80)throw std::invalid_argument("pressure produces carrier speed above model limit");}
  flows.push_back(q);if(r.phase==0)input+=q;else state.transfer_m3s=q;
 }
 if(branch!=gates.size()||outputs!=1)throw std::invalid_argument("regulated route topology mismatch");
 // Commit only after every setting/route/flow passed validation.
 for(size_t i=0;i<routes.size();++i)for(int at:routes[i].cells)d[at+2].x=float(flows[i]);
 for(size_t i=0;i<gates.size();++i){d[gates[i]+1].y=(suppliers&(std::uint32_t(1)<<i))?1.f:0.f;d[gates[i]+1].z=1;}
 d[wh+1].y=hopper?1.f:0.f;d[wh+1].z=1;d[rh+1].y=receiver_open?1.f:0.f;d[rh+1].z=1;
 d[4].y=1;d[4].z=float(target.weighing);d[4].w=float(state.applied.receiver);state.weighing_regulator_m3s=state.transfer_m3s-input;
 return state;
}
}
