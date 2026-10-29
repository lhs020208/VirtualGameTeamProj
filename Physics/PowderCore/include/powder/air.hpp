#pragma once
#include "material.hpp"
#include <ostream>
#include <iomanip>
namespace powder {
// Uniform prescribed gas; isolated rigid spheres, SI/local axes. Not CFD.
struct GranularAir {
 bool enabled=false;
 double density_kg_m3=1.225, viscosity_pa_s=1.81e-5;
 std::array<double,3> velocity_m_s{};
 void validate() const {
  if(!enabled)return;
  if(!std::isfinite(density_kg_m3)||density_kg_m3<.01||density_kg_m3>100||
     !std::isfinite(viscosity_pa_s)||viscosity_pa_s<1.e-7||viscosity_pa_s>1.e-3)
   throw std::invalid_argument("air requires density 0.01..100 kg/m3 and viscosity 1e-7..1e-3 Pa s");
  double speed2=0;for(double v:velocity_m_s){if(!std::isfinite(v))throw std::invalid_argument("nonfinite air velocity");speed2+=v*v;}
  if(speed2>10000)throw std::invalid_argument("prescribed air speed exceeds 100 m/s contract");
 }
 void validate_material(const GranularMaterial& m) const {
  if(!enabled)return;
  validate();m.validate();
  if(m.source_diameter_m>0&&std::abs(m.source_diameter_m-2*m.radius_m)>2*m.radius_m*1.e-6)
   throw std::invalid_argument("air coarse-graining unsupported: source and computational diameters must match (zero source means virtual sphere)");
 }
};
// beta = F_drag/(mass * slip). Cd*Re form retains the finite Stokes limit.
inline double sphere_drag_beta(const GranularAir& air,double radius,double density,double slip){
 if(!std::isfinite(radius)||radius<=0||!std::isfinite(density)||density<=0||!std::isfinite(slip)||slip<0)
  throw std::invalid_argument("invalid sphere drag state");
 const double re=air.density_kg_m3*(2*radius)*slip/air.viscosity_pa_s;
 if(!std::isfinite(re)||re>200000||slip>100)throw std::invalid_argument("air slip/Re outside isolated-sphere low-Mach contract");
 const double cd_re=re<=1000?24*(1+.15*std::pow(re,.687)):.44*re;
 return 3*air.viscosity_pa_s*cd_re/(16*density*radius*radius);
}
inline std::array<double,3> advance_sphere_air(const GranularAir& air,double radius,double density,
 std::array<double,3> velocity,double dt,std::array<double,3> gravity={0,0,-9.81}){
 if(!std::isfinite(dt)||dt<=0)throw std::invalid_argument("invalid air dt");
 for(int k=0;k<3;++k)if(!std::isfinite(velocity[k])||!std::isfinite(gravity[k]))throw std::invalid_argument("nonfinite air state");
 if(!air.enabled){for(int k=0;k<3;++k)velocity[k]+=dt*gravity[k];return velocity;}
 air.validate();double slip2=0;for(int k=0;k<3;++k)slip2+=std::pow(velocity[k]-air.velocity_m_s[k],2);
 const double beta=sphere_drag_beta(air,radius,density,std::sqrt(slip2));
 if(!std::isfinite(beta*dt)||beta*dt>.25)throw std::invalid_argument("air relaxation unresolved: reduce dt (beta*dt must be <=0.25)");
 for(int k=0;k<3;++k)velocity[k]=(velocity[k]+dt*(gravity[k]*(1-air.density_kg_m3/density)+beta*air.velocity_m_s[k]))/(1+dt*beta);
 return velocity;
}
inline void write_granular_air(std::ostream& out,const GranularAir& a){
 out<<std::setprecision(17)<<"{\"enabled\":"<<(a.enabled?"true":"false")
    <<",\"model\":\"prescribed_uniform_sphere_SN\",\"two_way\":false,\"dense_flow_validated\":false,\"buoyancy\":true"
    <<",\"density_kg_m3\":"<<a.density_kg_m3<<",\"viscosity_pa_s\":"<<a.viscosity_pa_s
    <<",\"velocity_local_m_s\":["<<a.velocity_m_s[0]<<','<<a.velocity_m_s[1]<<','<<a.velocity_m_s[2]
    <<"],\"diameter_basis\":\"computational_sphere\",\"max_beta_dt\":0.25,\"max_Re\":200000}";
}
}
