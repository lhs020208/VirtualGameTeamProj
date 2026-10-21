#pragma once
#include "powder/settings.hpp"
namespace powder {
struct SupplyReport {
 bool configured=false,complete=false;
 std::size_t planned=0,requested=0,inserted=0,pending=0,future=0;
 double planned_mass=0,requested_mass=0,inserted_mass=0,pending_mass=0,future_mass=0,mass_error=0;
 double last_insertion_s=-1;
};
// Finite inventory. FIFO; no particle is resized, dropped or forced through a blockage.
class Supply {
public:
 Supply(const PowderSettings&,ParticleSystem geometry,const std::function<bool()>& cancelled={});
 bool needs_poll(double time) const;
 // Transactional world and inventory update. Births occur at this physical time.
 std::vector<Particle> poll(ParticleSystem&,double time,const std::function<bool()>& cancelled={});
 const SupplyReport& report()const{return report_;}
 const InitialReport& initial()const{return initial_;}
 ParticleSystem empty_world()const;
private:
 PowderSettings settings_;ParticleSystem prepared_;InitialReport initial_;
 SupplyReport report_;double next_retry_=0,last_poll_=-1;
 double due(std::size_t index)const;
 void totals();
};
}
