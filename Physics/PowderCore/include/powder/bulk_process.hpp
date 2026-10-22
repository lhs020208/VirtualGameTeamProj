#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <limits>
namespace powder {
using SpeciesMass=std::array<double,3>;
inline double bulk_sum(const SpeciesMass& m){return m[0]+m[1]+m[2];}
inline void bulk_require(bool b,const char* s){if(!b)throw std::invalid_argument(s);}
struct BulkNode {SpeciesMass mass{};double capacity=1;};
struct BulkFlux {std::size_t from=0,to=0;double requested=0,actual=0;};
// Simultaneous transfers: requests see the old composition, never newly received mass.
class BulkGraph {
public:
 std::vector<BulkNode> nodes;
 SpeciesMass totals()const{SpeciesMass t{};for(const auto& n:nodes)for(unsigned g=0;g<3;++g)t[g]+=n.mass[g];return t;}
 void transfer(std::vector<BulkFlux>& flows){
  std::vector<double> outgoing(nodes.size(),0),incoming(nodes.size(),0),scale(nodes.size(),1);
  for(const auto& n:nodes){bulk_require(std::isfinite(n.capacity)&&n.capacity>0,"invalid node capacity");for(double m:n.mass)bulk_require(std::isfinite(m)&&m>=0,"invalid node mass");bulk_require(bulk_sum(n.mass)<=n.capacity+1e-9,"over capacity");}
  for(const auto& f:flows){bulk_require(f.from<nodes.size()&&f.to<nodes.size()&&f.from!=f.to&&std::isfinite(f.requested)&&f.requested>=0,"invalid flux");outgoing[f.from]+=f.requested;bulk_require(std::isfinite(outgoing[f.from]),"flux overflow");}
  for(std::size_t i=0;i<nodes.size();++i)if(outgoing[i]>0)scale[i]=std::min(1.,bulk_sum(nodes[i].mass)/outgoing[i]);
  for(const auto& f:flows)incoming[f.to]+=f.requested*scale[f.from];
  auto next=nodes;
  for(auto& f:flows){double room=std::max(0.,nodes[f.to].capacity-bulk_sum(nodes[f.to].mass));double target=incoming[f.to]>0?std::min(1.,room/incoming[f.to]):0;double amount=f.requested*scale[f.from]*target;f.actual=amount;double donor=bulk_sum(nodes[f.from].mass);if(donor<=0)continue;for(unsigned g=0;g<3;++g){double m=amount*(nodes[f.from].mass[g]/donor);next[f.from].mass[g]-=m;next[f.to].mass[g]+=m;}}
  // Roundoff only: avoid a negative zero-scale remainder without changing positive masses.
  for(auto& n:next)for(double& m:n.mass){bulk_require(m>=-1e-12&&std::isfinite(m),"invalid transfer result");if(m<0)m=0;}
  nodes.swap(next);
 }
};
enum class BulkPhase {Weigh,Settle,Transfer,Vent,Dump,Mix,Complete};
inline const char* bulk_phase_name(BulkPhase p){switch(p){case BulkPhase::Weigh:return "weigh";case BulkPhase::Settle:return "settle";case BulkPhase::Transfer:return "vacuum_transfer";case BulkPhase::Vent:return "pressure_equalize";case BulkPhase::Dump:return "receiver_dump";case BulkPhase::Mix:return "mix";default:return "complete";}}
struct BulkProcessConfig {
 double total_kg=30,reserve_factor=1,feed_kg_s=2,fine_feed_kg_s=.5,sensor_tau=.05,valve_tau=.08;
 double ambient_pa=101325,pump_target_pa=90000,gas_volume_m3=.02,inlet_conductance=2e-6,pump_conductance=2e-5,vent_conductance=2e-5;
 double gas_rt=86115,pipe_length_m=2,pipe_area_m2=.007853981633974483,bulk_density=800,particle_tau=.15,loading_factor=.3;
 unsigned pipe_cells=24;double dump_kg_s=3,rpm=30,mix_seconds=30,mix_exchange=.08,dead_zone=.15;
};
struct BulkControls {bool supply_enabled=true,transfer_open=true,pump_enabled=true,dump_open=true;double rpm=30;};
class BulkProcess {
public:
 const BulkProcessConfig config;BulkGraph graph;BulkControls controls;
 BulkPhase phase=BulkPhase::Weigh;unsigned ingredient=0;double time=0,phase_time=0,sensor_kg=0,feed_opening=0,pressure_pa=0,air_flow_kg_s=0,particle_speed=0;
 std::vector<double> pipe_travel;double mix_turns=0;
 double mix_time=0,torque_estimate_nm=0,gas_in_kg=0,gas_out_kg=0;std::vector<BulkFlux> last_flows;SpeciesMass initial{};
 std::size_t weigh=3,pipe=4,receiver=0,mixer=0;static constexpr unsigned mix_cells=24;
 explicit BulkProcess(BulkProcessConfig c):config(c){
  for(double v:{c.total_kg,c.feed_kg_s,c.fine_feed_kg_s,c.sensor_tau,c.valve_tau,c.ambient_pa,c.pump_target_pa,c.gas_volume_m3,c.inlet_conductance,c.pump_conductance,c.vent_conductance,c.gas_rt,c.pipe_length_m,c.pipe_area_m2,c.bulk_density,c.particle_tau,c.dump_kg_s,c.mix_seconds})bulk_require(std::isfinite(v)&&v>0,"invalid process config");
  bulk_require(std::isfinite(c.reserve_factor)&&c.reserve_factor>=1&&c.reserve_factor<=2,"invalid reserve factor");
  bulk_require(c.total_kg<=50&&c.pipe_cells>=4&&c.pipe_cells<=128&&c.pump_target_pa<c.ambient_pa&&c.pump_target_pa>=.8*c.ambient_pa,"outside prototype pressure/mass domain");
  bulk_require(std::isfinite(c.rpm)&&c.rpm>=0&&c.rpm<=120&&std::isfinite(c.mix_exchange)&&c.mix_exchange>=0&&std::isfinite(c.dead_zone)&&c.dead_zone>=0&&c.dead_zone<=1&&std::isfinite(c.loading_factor)&&c.loading_factor>=0,"invalid mixing/drag config");
  pipe_travel.resize(c.pipe_cells,0);receiver=pipe+c.pipe_cells;mixer=receiver+1;graph.nodes.resize(mixer+mix_cells);for(auto& n:graph.nodes)n.capacity=60;
  for(unsigned g=0;g<3;++g){graph.nodes[g].mass[g]=c.total_kg/3*c.reserve_factor;initial[g]=c.total_kg/3*c.reserve_factor;}
  for(unsigned i=0;i<c.pipe_cells;++i)graph.nodes[pipe+i].capacity=c.pipe_length_m*c.pipe_area_m2*c.bulk_density/c.pipe_cells;
  for(unsigned i=0;i<mix_cells;++i)graph.nodes[mixer+i].capacity=60./mix_cells;
  pressure_pa=c.ambient_pa;controls.rpm=c.rpm;
 }
 double pipe_mass()const{double m=0;for(unsigned i=0;i<config.pipe_cells;++i)m+=bulk_sum(graph.nodes[pipe+i].mass);return m;}
 SpeciesMass mixer_mass()const{SpeciesMass result{};for(unsigned i=0;i<mix_cells;++i)for(unsigned g=0;g<3;++g)result[g]+=graph.nodes[mixer+i].mass[g];return result;}
 double mixing_index()const{
  auto totals=mixer_mass();double mass=bulk_sum(totals);if(mass<=0)return 0;double denom=0,variance=0;for(unsigned g=0;g<3;++g){double f=totals[g]/mass;denom+=f*(1-f);}
  if(denom<=0)return 0;for(unsigned i=0;i<mix_cells;++i){const auto& m=graph.nodes[mixer+i].mass;double local=bulk_sum(m);if(local<=0)continue;for(unsigned g=0;g<3;++g){double delta=m[g]/local-totals[g]/mass;variance+=local/mass*delta*delta;}}return std::clamp(1-variance/denom,0.,1.);
 }
 void advance(double dt){bulk_require(std::isfinite(dt)&&dt>0&&dt<=.02&&std::isfinite(controls.rpm)&&controls.rpm>=0&&controls.rpm<=120,"invalid step/controls");if(phase==BulkPhase::Complete)return;
  // All internal steps <=0.005 s; requested physical time is never dropped.
  double remaining=dt;while(remaining>1e-15&&phase!=BulkPhase::Complete){double max_air=config.inlet_conductance*(config.ambient_pa-config.pump_target_pa)/(config.pump_target_pa/config.gas_rt*config.pipe_area_m2);double h=std::min({remaining,.005,.4*(config.pipe_length_m/config.pipe_cells)/std::max({max_air,std::abs(particle_speed),1e-9})});step(h);remaining-=h;}
 }
private:
 void change(BulkPhase next){phase=next;phase_time=0;}
 void step(double h){
  time+=h;phase_time+=h;last_flows.clear();const bool sucking=phase==BulkPhase::Transfer&&controls.transfer_open;
  double ci=sucking?config.inlet_conductance/(1+config.loading_factor*pipe_mass()):0;
  double cp=phase==BulkPhase::Transfer&&controls.pump_enabled?config.pump_conductance:0;
  double cv=phase==BulkPhase::Vent||phase==BulkPhase::Dump?config.vent_conductance:0;
  double k=ci+cp+cv;
  // Isothermal gas storage: dP/dt=RT/V * (Cin(Pambient-P)-Cp(P-Ptarget)).
  
  if(k>0){double equilibrium=((ci+cv)*config.ambient_pa+cp*config.pump_target_pa)/k;double a=config.gas_rt/config.gas_volume_m3*k;double integral=equilibrium*h+(pressure_pa-equilibrium)*(-std::expm1(-a*h))/a;gas_in_kg+=(ci+cv)*(config.ambient_pa*h-integral);gas_out_kg+=cp*(integral-config.pump_target_pa*h);pressure_pa=equilibrium+(pressure_pa-equilibrium)*std::exp(-a*h);}
  air_flow_kg_s=ci*(config.ambient_pa-pressure_pa);
  double density=.5*(config.ambient_pa+pressure_pa)/config.gas_rt;double air_speed=air_flow_kg_s/(density*config.pipe_area_m2);
  particle_speed=air_speed+(particle_speed-air_speed)*std::exp(-h/config.particle_tau);
  double sensor_target=bulk_sum(graph.nodes[weigh].mass);sensor_kg=sensor_target+(sensor_kg-sensor_target)*std::exp(-h/config.sensor_tau);
  double command=phase==BulkPhase::Weigh&&controls.supply_enabled?1:0;
  feed_opening=command+(feed_opening-command)*std::exp(-h/config.valve_tau);
  if(phase==BulkPhase::Weigh||phase==BulkPhase::Settle){double target=config.total_kg/3;double feed=target-sensor_kg<.2?config.fine_feed_kg_s:config.feed_kg_s;last_flows.push_back({ingredient,weigh,feed*feed_opening*h,0});}
  if(sucking){double dx=config.pipe_length_m/config.pipe_cells;double fraction=std::min(.4,std::max(0.,particle_speed)*h/dx);
   // Positive gas pressure drive only; no transfer through a shut valve or without suction.
   if(air_flow_kg_s>1e-12){last_flows.push_back({weigh,pipe,std::min(config.feed_kg_s*h,fraction*graph.nodes[pipe].capacity),0});for(unsigned i=0;i<config.pipe_cells;++i)last_flows.push_back({pipe+i,i+1<config.pipe_cells?pipe+i+1:receiver,fraction*bulk_sum(graph.nodes[pipe+i].mass),0});}
  }
  if(phase==BulkPhase::Dump&&controls.dump_open&&std::abs(pressure_pa-config.ambient_pa)<500){for(unsigned j=0;j<8;++j)last_flows.push_back({receiver,mixer+ingredient*8+j,config.dump_kg_s*h/8,0});}
  std::vector<double> before_pipe(config.pipe_cells);for(unsigned i=0;i<config.pipe_cells;++i)before_pipe[i]=bulk_sum(graph.nodes[pipe+i].mass);
  graph.transfer(last_flows);
  for(const auto& f:last_flows)if(f.from>=pipe&&f.from<receiver&&f.actual>0){auto i=f.from-pipe;if(before_pipe[i]>0)pipe_travel[i]+=f.actual/before_pipe[i]*(config.pipe_length_m/config.pipe_cells);}
  if(phase==BulkPhase::Weigh&&(sensor_kg>=config.total_kg/3-config.fine_feed_kg_s*(config.sensor_tau+config.valve_tau)||bulk_sum(graph.nodes[ingredient].mass)<1e-9))change(BulkPhase::Settle);
  else if(phase==BulkPhase::Settle&&phase_time>=.5)change(BulkPhase::Transfer);
  else if(phase==BulkPhase::Transfer&&bulk_sum(graph.nodes[weigh].mass)+pipe_mass()<1e-7)change(BulkPhase::Vent);
  else if(phase==BulkPhase::Vent&&std::abs(pressure_pa-config.ambient_pa)<500)change(BulkPhase::Dump);
  else if(phase==BulkPhase::Dump&&bulk_sum(graph.nodes[receiver].mass)<1e-7){if(ingredient<2){++ingredient;change(BulkPhase::Weigh);}else change(BulkPhase::Mix);}
  else if(phase==BulkPhase::Mix){mix(h);if(controls.rpm>0){mix_time+=h;mix_turns+=controls.rpm*h/60;}if(mix_time>=config.mix_seconds)change(BulkPhase::Complete);}
 }
 void mix(double h){double fill=bulk_sum(mixer_mass())/60.;double k=config.mix_exchange*(controls.rpm/30.)*std::max(.1,1-.5*fill);torque_estimate_nm=.01*bulk_sum(mixer_mass())*controls.rpm;
  // Pairwise exact diffusion, no flux through vessel boundary. Equal cell capacities.
  for(unsigned i=0;i<mix_cells;++i){unsigned x=i%4,y=(i/4)%3,z=i/12;for(unsigned axis=0;axis<3;++axis){if((axis==0&&x==3)||(axis==1&&y==2)||(axis==2&&z==1))continue;unsigned j=i+(axis==0?1:axis==1?4:12);double local=(x==0&&y==0)?config.dead_zone:1.;double fraction=.5*(1-std::exp(-2*k*local*h));auto a=graph.nodes[mixer+i].mass,b=graph.nodes[mixer+j].mass;for(unsigned g=0;g<3;++g){double moved=fraction*(a[g]-b[g]);graph.nodes[mixer+i].mass[g]-=moved;graph.nodes[mixer+j].mass[g]+=moved;}}}
 }
};
}
