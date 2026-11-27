#pragma once
#include "powder/granular.hpp"
#include <algorithm>

namespace powder {
// Empty equipment uses the same bounded drive equations, with zero particle load.
// No OpenCL context, particle buffers, collision queries or GPU kernels are created.
class EmptyPlantDrive {
 GranularConfig config;
 std::vector<int> gate_records;
 std::vector<bool> targets;
 double reference=0;
public:
 std::vector<PlantActuatorState> valves;
 PlantMixerDriveState mixer;
 explicit EmptyPlantDrive(const GranularConfig& c):config(c){
  if(!std::isfinite(c.dt)||c.dt<=0||c.dt>.00101f)throw std::invalid_argument("empty equipment dt must be <=1 ms");
  // Validate the ratings using a contact-safe validation interval; there are no contacts here.
  c.small_valve.validate(1.e-6f,1);c.large_valve.validate(1.e-6f,1);c.mixer_drive.validate(1.e-6f,1);
  int count=int(c.plant_data[5].y),base=int(c.plant_data[5].x);
  for(int j=0;j<count;++j){gate_records.push_back(base+2*j+1);targets.push_back(false);}
  reset();
 }
 void reset(){
  reference=0;mixer={};mixer.supported=config.plant_powered_mixer;mixer.power_fraction=config.mixer_drive.supply_fraction;
  valves.assign(config.plant_powered_valves?gate_records.size():0,{});
  for(size_t i=0;i<valves.size();++i){auto c=config.plant_data[gate_records[i]-1].w>.06f?config.large_valve:config.small_valve;valves[i].power_fraction=c.supply_fraction;valves[i].voltage=c.voltage;}
  std::fill(targets.begin(),targets.end(),false);
 }
 void controls(const std::vector<Grain>& d){for(size_t i=0;i<targets.size();++i)targets[i]=d.at(gate_records[i]).y>0;}
 void advance(double dt,double rpm){
  constexpr double halfpi=1.5707963267948966;
  for(size_t j=0;j<valves.size();++j){auto& s=valves[j];const auto c=config.plant_data[gate_records[j]-1].w>.06f?config.large_valve:config.small_valve;
   const double target=targets[j]?halfpi:0;
   if(std::abs(target-s.target_rad)>1.e-6||s.power_fraction==0){s.stalled_seconds=0;s.tripped=false;}s.target_rad=float(target);
   double err=target-s.angle_rad,rate=halfpi/c.travel_seconds,load=c.external_torque_nm,cap=c.torque_nm*s.power_fraction;
   bool powered=s.power_fraction>0&&!s.tripped&&std::abs(err)>.00001;
   double desired=std::copysign(std::min(rate,std::sqrt(2*c.torque_nm/c.inertia_kg_m2*std::abs(err))),err),gain=c.torque_nm/(.15*rate);
   double span=dt*(c.torque_nm+c.holding_nm+std::abs(load))/c.inertia_kg_m2,lo=s.omega_rad_s-span,hi=s.omega_rad_s+span,motor=0,omega=0;
   for(int k=0;k<40;++k){omega=(lo+hi)/2;double limit=std::min(cap,.35*c.electrical_w*s.power_fraction/std::max(std::abs(omega),1.e-6));
    motor=powered?std::clamp(gain*(desired-omega),-limit,limit):std::clamp(-gain*omega,-double(c.holding_nm),double(c.holding_nm));
    if(c.inertia_kg_m2*(omega-s.omega_rad_s)/dt-motor-load>0)hi=omega;else lo=omega;
   }
   if(!powered&&std::abs(s.omega_rad_s)<rate*.001&&std::abs(load)<=c.holding_nm){omega=0;motor=-load;}
   double angle=s.angle_rad+omega*dt;if(angle<=0){angle=0;omega=std::max(0.,omega);}if(angle>=halfpi){angle=halfpi;omega=std::min(0.,omega);}
   if(powered&&std::abs(err)>.02&&std::abs(omega)<rate*.02&&std::abs(motor)>cap*.95)s.stalled_seconds+=float(dt);else s.stalled_seconds=0;
   if(s.stalled_seconds>=c.trip_seconds)s.tripped=true;
   double watts=powered?std::min(double(c.electrical_w*s.power_fraction),c.electrical_w*s.power_fraction*(.2+.8*std::pow(std::abs(motor)/std::max(cap,1.e-6),2))+std::max(0.,motor*omega)/.35):0;
   s.angle_rad=float(angle);s.omega_rad_s=float(omega);s.motor_nm=float(motor);s.electrical_w=float(watts);s.energy_j+=float(watts*dt);
  }
  if(!mixer.supported){mixer.angle_rad+=rpm*.10471975511965977*dt;return;}
  auto& s=mixer;const auto& c=config.mixer_drive;double request=rpm*.10471975511965977;
  if(request==0||s.power_fraction==0){s.stalled_seconds=0;s.tripped=false;}
  bool powered=s.power_fraction>0&&!s.tripped&&(request!=0||s.omega_rad_s!=0);
  reference=powered?reference+std::clamp(request-reference,-halfpi/c.ramp_seconds*dt,halfpi/c.ramp_seconds*dt):0;
  auto torque=[&](double omega){if(!powered)return 0.;double cap=std::min(double(c.torque_nm*s.power_fraction),c.efficiency*c.input_w*s.power_fraction/std::max(std::abs(omega),1.e-6));return std::clamp(c.torque_nm/(.1*halfpi)*(reference-omega),-cap,cap);};
  double dry=c.test_resisting_nm+(!powered||request==0?c.brake_nm:0),omega=0,motor=torque(0);
  if(std::abs(c.inertia_kg_m2*s.omega_rad_s/dt+motor)>dry){
   double span=dt*(c.torque_nm+dry+c.bearing_nm_s*std::abs(s.omega_rad_s))/c.inertia_kg_m2+std::abs(s.omega_rad_s),lo=-span,hi=span;
   for(int k=0;k<40;++k){omega=(lo+hi)/2;motor=torque(omega);double residual=c.inertia_kg_m2*(omega-s.omega_rad_s)/dt-motor+c.bearing_nm_s*omega+std::copysign(dry,omega);if(residual>0)hi=omega;else lo=omega;}
  }
  double cap=c.torque_nm*s.power_fraction;
  if(powered&&std::abs(request)>.01&&std::abs(omega)<std::abs(request)*.02&&std::abs(motor)>cap*.95)s.stalled_seconds+=float(dt);else s.stalled_seconds=0;
  if(s.stalled_seconds>=c.trip_seconds)s.tripped=true;
  double watts=powered?std::max(0.,motor*omega)+(1-c.efficiency)*c.input_w*s.power_fraction*(.1+.9*std::pow(std::abs(motor)/std::max(cap,1.e-6),2)):0;
  s.target_rad_s=float(request);s.omega_rad_s=float(omega);s.angle_rad+=omega*dt;s.motor_nm=float(motor);s.electrical_w=float(watts);s.energy_j+=float(watts*dt);
 }
};
}
