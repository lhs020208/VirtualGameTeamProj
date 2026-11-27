#pragma once
#include <cmath>
#include <algorithm>
#include <initializer_list>
#include <stdexcept>
namespace powder {
// Representative electric quarter-turn gearbox, not the installed pneumatic unit.
// Valpes ER PLUS: ER10.X0B.G00 10 Nm/15 W/11 s; ER100.90B.G00 100 Nm/45 W/24 s.
// Inertia, servo droop, contact stiffness and protection delay are explicit trial assumptions.
struct PlantActuatorSettings {
 float torque_nm=10, electrical_w=15, travel_seconds=11, inertia_kg_m2=.1f;
 float voltage=24, stiffness_n_m=5000, damping_ratio=.7f, holding_nm=10;
 float supply_fraction=1, trip_seconds=2, external_torque_nm=0;
 void validate(float dt,float mass)const {
  for(float x:{torque_nm,electrical_w,travel_seconds,inertia_kg_m2,voltage,stiffness_n_m,trip_seconds})
   if(!std::isfinite(x)||x<=0)throw std::invalid_argument("invalid powered valve parameter");
  if(!std::isfinite(damping_ratio)||damping_ratio<0||damping_ratio>1||!std::isfinite(holding_nm)||holding_nm<0||!std::isfinite(external_torque_nm)||!std::isfinite(supply_fraction)||supply_fraction<0||supply_fraction>1)
   throw std::invalid_argument("invalid powered valve load/power");
  if(dt*std::sqrt(stiffness_n_m/mass)>.25f)throw std::invalid_argument("valve contact timestep too large: reduce dt");
 }
 static PlantActuatorSettings large(){PlantActuatorSettings v;v.torque_nm=100;v.electrical_w=45;v.travel_seconds=24;v.inertia_kg_m2=1;v.holding_nm=100;return v;}
};
struct PlantActuatorState {
 float angle_rad=0,omega_rad_s=0,target_rad=0,power_fraction=0;
 float motor_nm=0,contact_nm=0,electrical_w=0,energy_j=0;
 float stalled_seconds=0;bool tripped=false;
 float normal_n=0,penetration_m=0,voltage=24;
 bool closed()const{return angle_rad<=.0001f&&std::abs(omega_rad_s)<.001f;}
 bool open()const{return angle_rad>=1.57079632679f-.0001f&&std::abs(omega_rad_s)<.001f;}
 double aperture()const{return closed()?0.:std::max(0.,1.-std::cos(double(angle_rad)));}
};
}
