#include "powder/granular_coupled.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace powder {
namespace {void ensure(bool value,const char* msg){if(!value)throw std::runtime_error(msg);}}
GranularGasCoupler::GranularGasCoupler(const GranularConfig& c,const std::vector<Grain>& initial,GasGrid g,Vec3 origin):gas(std::move(g)),config_(c){
 ensure(c.coupled_air&&c.binary_materials&&!c.air.enabled&&initial.size()==c.count,"coupled GPU configuration mismatch");for(const auto& m:c.materials)m.validate();
 const double h=gas.config.h,v=h*h*h;std::vector<double> alpha(gas.cells.size());
 for(const auto& p:initial){ensure(p.w==0||p.w==1,"invalid coupled group");const auto& m=c.materials[p.w==0?0:1];double vp=m.mass_kg()/m.density_kg_m3;volume_+=vp;for(auto a:gas.stencil({p.x-origin.x,p.y-origin.y,p.z-origin.z}))alpha[a.first]+=a.second*vp/v;}
 for(std::size_t i=0;i<alpha.size();++i){ensure(alpha[i]<=.65,"initial coupled porosity below 0.35");gas.cells[i].epsilon=1-alpha[i];}gas.initialize_pressure();initial_mass_=gas.mass();field_.cells=gas.config.cells;field_.origin={float(origin.x),float(origin.y),float(origin.z)};field_.h=float(h);field_.viscosity=float(gas.config.viscosity());field_.velocity_density.resize(gas.cells.size());field_.gradient_epsilon.resize(gas.cells.size());
}
double GranularGasCoupler::gas_mass_error()const{return gas.mass()+gas.boundary_mass_out-initial_mass_;}
void GranularGasCoupler::advance(GranularGPU& gpu,unsigned steps,float rpm,bool gate){ensure(!failed_&&steps>0&&steps<=1000,"coupled advance invalid; reset required after failure");try{for(unsigned i=0;i<steps;++i)step(gpu,rpm,gate);}catch(...){failed_=true;throw;}}
void GranularGasCoupler::step(GranularGPU& gpu,float rpm,bool gate){
 const auto dims=gas.config.cells;ensure(dims==field_.cells&&std::abs(gas.config.h-field_.h)<1e-8,"gas grid geometry changed; reset required");const double h=gas.config.h,v=h*h*h;double remaining=config_.dt;
 while(remaining>1e-14){double dt=std::min(remaining,gas.explicit_dt_limit());gas.step(dt,{0,0,-9.81});remaining-=dt;}
 field_.viscosity=float(gas.config.viscosity());
 for(int z=0;z<dims[2];++z)for(int y=0;y<dims[1];++y)for(int x=0;x<dims[0];++x){int i=gas.index(x,y,z),q[3]{x,y,z};if(gas.cells[i].solid){field_.velocity_density[i]={0,0,0,1.2f};field_.gradient_epsilon[i]={};continue;}auto u=gas.velocity(i);field_.velocity_density[i]={float(u.x),float(u.y),float(u.z),float(gas.cells[i].mass/(gas.cells[i].epsilon*v))};Grain a{0,0,0,float(gas.cells[i].epsilon)};float* gradient=&a.x;double pc=gas.pressure(i);
  for(int k=0;k<3;++k){double pv[2];for(int side=0;side<2;++side){int at[3]{x,y,z};at[k]+=side?1:-1;if(at[k]<0||at[k]>=dims[k]){double p=gas.config.boundary_pa[2*k+side];pv[side]=p>0?2*p-pc:pc;}else{int j=gas.index(at[0],at[1],at[2]);pv[side]=gas.cells[j].solid?pc:gas.pressure(j);}}gradient[k]=float((pv[1]-pv[0])/(2*h));}field_.gradient_epsilon[i]=a;
 }
 gpu.set_gas_field(field_);gpu.advance(1,rpm,gate);auto exchange=gpu.gas_exchange();ensure(exchange.steps==1,"coupled exchange clock mismatch");double mapped=0;
 for(std::size_t i=0;i<exchange.impulse_volume.size();++i){auto a=exchange.impulse_volume[i];double alpha=a.w/v;ensure(std::isfinite(alpha)&&alpha>=0&&alpha<=.65,"coupled porosity outside contract");if(gas.cells[i].solid){ensure(a.x==0&&a.y==0&&a.z==0&&a.w==0,"gas solid cell received impulse");continue;}ensure(config_.dt*exchange.drag_rate_kg_s[i]/gas.cells[i].mass<=.1,"gas drag feedback timestep unresolved; reduce particle dt");gas.cells[i].epsilon=1-alpha;gas.cells[i].momentum.x+=a.x;gas.cells[i].momentum.y+=a.y;gas.cells[i].momentum.z+=a.z;mapped+=a.w;auto u=gas.velocity(int(i));double speed=std::hypot(u.x,u.y,u.z);ensure(std::isfinite(speed)&&speed<=100,"coupled post-exchange gas speed outside contract");max_speed_=std::max(max_speed_,speed);}
 volume_error_=mapped-volume_;ensure(std::abs(volume_error_)<=volume_*2e-5,"coupled volume ledger mismatch");ensure(std::abs(gas_mass_error())<=std::max(1e-12,initial_mass_*1e-8),"coupled gas mass ledger mismatch");elapsed_+=config_.dt;
}
}
