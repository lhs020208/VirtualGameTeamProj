#pragma once
#include <cmath>
#include <initializer_list>
#include <stdexcept>
namespace powder {
// Unilateral rigid stops derived from fixed obstacle geometry at reset. Angles
// are unwrapped about the initial shaft pose, in the solver's world-X convention.
struct PlantMixerStops {
 bool enabled=false;
 float lower_rad=0,upper_rad=0;
 void validate()const {
  if(enabled&&(!std::isfinite(lower_rad)||!std::isfinite(upper_rad)||
     lower_rad>0||upper_rad<0||lower_rad>=upper_rad))
   throw std::invalid_argument("invalid mixer obstacle angular clearance");
 }
};
// Trial VFD/gearmotor equivalent: SEW DRN90S4 1.1 kW / 84.5% motor efficiency,
// R87 table around 14 rpm / 750 Nm. Gear efficiency, inertia and ramp are assumptions.
struct PlantMixerDriveSettings {
 float input_w=1302,torque_nm=750,efficiency=.8f,inertia_kg_m2=60;
 float supply_fraction=1,ramp_seconds=2,brake_nm=100,trip_seconds=2;
 float stiffness_n_m=5000,damping_ratio=.7f,bearing_nm_s=2;
 float test_resisting_nm=0; // Coulomb load fixture; never enabled by the level.
 void validate(float dt,float mass)const {
  for(float v:{input_w,torque_nm,inertia_kg_m2,ramp_seconds,trip_seconds,stiffness_n_m})
   if(!std::isfinite(v)||v<=0)throw std::invalid_argument("invalid mixer drive parameter");
  for(float v:{brake_nm,bearing_nm_s,test_resisting_nm})if(!std::isfinite(v)||v<0)throw std::invalid_argument("invalid mixer resistance");
  if(!std::isfinite(efficiency)||efficiency<=0||efficiency>=1||!std::isfinite(supply_fraction)||supply_fraction<0||supply_fraction>1||!std::isfinite(damping_ratio)||damping_ratio<0||damping_ratio>1)
   throw std::invalid_argument("invalid mixer supply/efficiency/damping");
  if(dt*std::sqrt(stiffness_n_m/mass)>.25f)throw std::invalid_argument("mixer contact timestep too large: reduce dt");
 }
};
struct PlantMixerDriveState {
 bool supported=false,tripped=false;
 double angle_rad=0;
 float omega_rad_s=0,target_rad_s=0,power_fraction=0;
 float motor_nm=0,contact_nm=0,electrical_w=0,energy_j=0,normal_n=0,penetration_m=0,stalled_seconds=0;
 double rpm()const{return omega_rad_s/0.10471975511965977;}
 bool stopped()const{return std::abs(omega_rad_s)<1.e-6f;}
};
}
