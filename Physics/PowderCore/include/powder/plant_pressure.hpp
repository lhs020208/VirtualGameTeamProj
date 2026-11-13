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
// A gas-only ideal pressure boundary at the first complete cell below supplier A's shut gate.
// The existing WH ideal regulator is the exhaust sink; no closed-gate gas bypass is introduced.
struct PlantSupplyAPurge {bool enabled=false;double gauge_pa=0;};
struct PlantSupplyAPurgePath {size_t route=0,first_cell=0;Grain inlet{};};
inline PlantSupplyAPurgePath plant_supply_a_purge_path(const std::vector<Grain>&d,const std::vector<PlantRoute>&routes){
 auto gates=plant_supplier_gates(d);
 if(gates.size()!=6)throw std::invalid_argument("A purge requires verified LowPoly six-supplier topology");
 auto gate=d[gates[0]];
 if(std::abs(gate.x+1.2f)>1e-5f||std::abs(gate.y-1.4f)>1e-5f||std::abs(gate.z-2.23f)>1e-5f)
  throw std::invalid_argument("A purge gate geometry mismatch");
 for(size_t ri=0;ri<routes.size();++ri)if(routes[ri].phase==0){
  for(size_t ci=0;ci<routes[ri].cells.size();++ci){int at=routes[ri].cells[ci];
   if(at<int(d[0].z)||size_t(at)+2>=d.size())throw std::invalid_argument("A purge route index invalid");
   auto a=d[at],b=d[at+1];
   if(a.z<gate.z&&b.z<a.z){
    if(std::abs(a.x-gate.x)>1e-5f||std::abs(a.y-gate.y)>1e-5f||std::abs(a.z-2.2f)>1e-5f)
     throw std::invalid_argument("A purge inlet geometry mismatch");
    return {ri,ci,a};
   }
  }break;
 }
 throw std::invalid_argument("A purge downstream route unavailable");
}
inline void validate_plant_supply_a_purge(PlantSupplyAPurge p){
 if(!std::isfinite(p.gauge_pa)||p.gauge_pa<0||p.gauge_pa>1000)
  throw std::invalid_argument("A purge trial boundary must be finite 0..1000 gauge Pa");
}
struct PlantPressureState {
 PlantPressureTargets applied;double weighing_regulator_m3s=0,transfer_m3s=0;
 PlantSupplyAPurge purge;double purge_m3s=0;
};
// The ideal WH regulator supplies q_out - sum(q_in); negative means extraction.
// This explicitly accounts for a reservoir source/sink, not a closed-node imbalance.
inline PlantPressureState plant_set_regulated_pressures(std::vector<Grain>&d,const std::vector<PlantRoute>&routes,PlantPressureTargets target,std::uint32_t suppliers,std::uint32_t supplier_air,bool hopper,bool hopper_air,bool receiver_open,PlantSupplyAPurge purge={}){
 validate_plant_pressure_targets(target);validate_plant_supply_a_purge(purge);
 if(d.size()<16||d[11].x!=0)throw std::invalid_argument("regulated pressure requires full plant");
 auto gates=plant_supplier_gates(d);int wh=plant_weighing_gate(d),rh=plant_receiver_gate(d);
 PlantPressureState state;state.applied=target;if(receiver_open)state.applied.receiver=0; // vented discharge test assumption
 PlantSupplyAPurgePath purge_path;
 if(purge.enabled||purge.gauge_pa!=0)purge_path=plant_supply_a_purge_path(d,routes);
 if(purge.enabled&&(suppliers||supplier_air||hopper||purge.gauge_pa<=target.weighing))
  throw std::invalid_argument("A purge requires closed suppliers/WH outlet and inlet pressure above WH");
 state.purge=purge;
 std::vector<std::vector<double>> flows;size_t branch=0,outputs=0,route_index=0;double input=0;
 for(const auto&r:routes){
  if(r.cells.empty()||(r.phase!=0&&r.phase!=1))throw std::invalid_argument("invalid regulated route");
  bool enabled;double dp;
  if(r.phase==0){if(branch>=gates.size())throw std::invalid_argument("too many supplier routes");enabled=(suppliers&supplier_air&(std::uint32_t(1)<<branch))!=0;++branch;dp=target.supply-target.weighing;}
  else{++outputs;enabled=hopper&&hopper_air;dp=target.weighing-state.applied.receiver;}
  const bool purging=purge.enabled&&route_index==purge_path.route;
  const size_t first=purging?purge_path.first_cell:0;
  if(purging){enabled=true;dp=purge.gauge_pa-target.weighing;}
  auto loss=[&](double q){double sum=0;for(size_t ci=first;ci<r.cells.size();++ci){int at=r.cells[ci];if(at<int(d[0].z)||at+2>=int(d.size())||(at-int(d[0].z))%3||d[at+2].w!=r.phase)throw std::invalid_argument("regulated route cell mismatch");auto a=d[at],b=d[at+1],v=d[at+2];double length=std::hypot(std::hypot(b.x-a.x,b.y-a.y),b.z-a.z);sum+=v.z==1?plant_cell_loss(q,length,2*a.w*b.w/(a.w+b.w),v.y,4*a.w*b.w):plant_cell_loss(q,length,std::min(a.w,b.w),v.y);}return sum;};
  loss(0);double q=0;
  if(enabled&&dp!=0){double lo=0,hi=1;if(loss(hi)<std::abs(dp))throw std::invalid_argument("pressure flow bracket exceeded");for(int n=0;n<55;++n){double mid=(lo+hi)/2;if(loss(mid)>std::abs(dp))hi=mid;else lo=mid;}q=std::copysign((lo+hi)/2,dp);}
  for(int at:r.cells){double radius=std::min(d[at].w,d[at+1].w);if(std::abs(q)/(3.141592653589793*radius*radius)>80)throw std::invalid_argument("pressure produces carrier speed above model limit");}
  std::vector<double> values(r.cells.size(),0);
  for(size_t ci=first;ci<values.size();++ci)values[ci]=q;
  flows.push_back(std::move(values));if(r.phase==0)input+=q;else state.transfer_m3s=q;
  if(purging)state.purge_m3s=q;
  ++route_index;
 }
 if(branch!=gates.size()||outputs!=1)throw std::invalid_argument("regulated route topology mismatch");
 // Commit only after every setting/route/flow passed validation.
 for(size_t i=0;i<routes.size();++i)for(size_t j=0;j<routes[i].cells.size();++j)d[routes[i].cells[j]+2].x=float(flows[i][j]);
 for(size_t i=0;i<gates.size();++i){d[gates[i]+1].y=(suppliers&(std::uint32_t(1)<<i))?1.f:0.f;d[gates[i]+1].z=1;}
 d[wh+1].y=hopper?1.f:0.f;d[wh+1].z=1;d[rh+1].y=receiver_open?1.f:0.f;d[rh+1].z=1;
 d[4].y=1;d[4].z=float(target.weighing);d[4].w=float(state.applied.receiver);state.weighing_regulator_m3s=state.transfer_m3s-input;
 return state;
}
}
