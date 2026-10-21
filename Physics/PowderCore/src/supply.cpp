#include "powder/supply.hpp"
#include <cmath>
#include <stdexcept>
namespace powder {
Supply::Supply(const PowderSettings& s,ParticleSystem geometry,const std::function<bool()>& cancelled):settings_(s){
 auto b=s.supply_mode==SupplyMode::Batch?make_initial(s,std::move(geometry),cancelled):make_inventory(s,std::move(geometry));
 prepared_=std::move(b.world);initial_=b.report;report_.configured=true;report_.planned=prepared_.particles.size();totals();
 // Validate shape fit even before the first scheduled request. Static occlusion stays pending in timed mode.
 if(s.supply_mode==SupplyMode::Timed){auto probe=empty_world();std::vector<Particle> one{prepared_.particles.front()};place_supply(s,probe,one,initial_.max_diameter_m,cancelled);}
}
ParticleSystem Supply::empty_world()const{auto w=prepared_;w.particles.clear();w.reset_history();return w;}
double Supply::due(std::size_t i)const{return settings_.supply_start_s+(settings_.supply_mode==SupplyMode::Batch?0:static_cast<double>(i/settings_.supply_per_event)*settings_.supply_interval_s);}
bool Supply::needs_poll(double time)const{
 if(report_.complete)return false;
 return (report_.requested<report_.planned&&time>=due(report_.requested))||(report_.pending&&time>=next_retry_);
}
void Supply::totals(){
 report_.pending=report_.requested-report_.inserted;report_.future=report_.planned-report_.requested;
 report_.planned_mass=report_.requested_mass=report_.inserted_mass=report_.pending_mass=report_.future_mass=0;
 for(std::size_t i=0;i<prepared_.particles.size();++i){auto& p=prepared_.particles[i];double m=sphere_properties(p.diameter_m,p.material).mass_kg;
  report_.planned_mass+=m;if(i<report_.requested)report_.requested_mass+=m;
  if(i<report_.inserted)report_.inserted_mass+=m;else if(i<report_.requested)report_.pending_mass+=m;else report_.future_mass+=m;
 }
 report_.mass_error=report_.planned_mass-report_.inserted_mass-report_.pending_mass-report_.future_mass;
 report_.complete=report_.inserted==report_.planned;
}
std::vector<Particle> Supply::poll(ParticleSystem& w,double time,const std::function<bool()>& cancelled){
 if(!std::isfinite(time)||time<0||time<last_poll_)throw std::invalid_argument("supply time must be finite nondecreasing");
 if(!needs_poll(time))return {};
 auto next=*this;auto candidate=w;
 while(next.report_.requested<next.report_.planned&&time>=due(next.report_.requested))++next.report_.requested;
 std::vector<Particle> waiting(prepared_.particles.begin()+report_.inserted,prepared_.particles.begin()+next.report_.requested);
 const auto old=candidate.particles.size();
 if(settings_.supply_mode==SupplyMode::Batch){
  // Batch has already been checked in the unchanged static rig, and no prior inventory was inserted.
  if(!candidate.particles.empty())throw std::invalid_argument("batch release requires empty world");
  candidate.particles=waiting;
 }else place_supply(settings_,candidate,waiting,initial_.max_diameter_m,cancelled);
 std::vector<Particle> born(candidate.particles.begin()+old,candidate.particles.end());
 next.report_.inserted+=born.size();if(!born.empty())next.report_.last_insertion_s=time;
 next.next_retry_=time+settings_.retry_interval_s;next.last_poll_=time;next.totals();
 validate_particles(candidate.particles);w=std::move(candidate);*this=std::move(next);return born;
}
}
