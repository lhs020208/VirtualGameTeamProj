#pragma once
#include "flow/solver.hpp"
#include "powder/core.hpp"

namespace flow {
inline constexpr const char* OneWayVersion="0.3.0-one-way";
struct DragResult {
    powder::Vec3 force_n;
    double reynolds=0,relaxation_time_s=0,knudsen_estimate=0;
};
// Isolated heavy nonrotating sphere, creeping continuum approximation.
// Gravity is external; buoyancy, lift, history, pressure-gradient and torque
// are excluded. Re<=0.1, Kn(d)<=0.01, density ratio>=100 are hard guards.
DragResult stokes_drag(const Contract& c,const powder::Particle& particle,powder::Vec3 air_velocity);
// Bilinear interpolation on each velocity component's staggered locations.
// Only the interior stencil domain is accepted; no edge clamping/extrapolation.
powder::Vec3 sample_velocity(const Contract& c,const Fields& f,powder::Vec3 position);
struct OneWaySettings {
    int particle_substeps=20;
    powder::Vec3 gravity_m_s2{0,-9.81,0}; // flow x/y == DEM x/y; z=0 plane
    bool drag_enabled=true;
};
struct OneWayState { Fields air;std::vector<powder::Particle> particles; };
struct OneWayReport {
    StepReport air;
    double max_particle_reynolds=0,max_dt_over_tau=0;
    powder::Vec3 drag_impulse_n_s;
};
// A whole air step and all particle substeps commit together. The air solver
// never reads particle state; drag_enabled=false is the gravity-only control.
OneWayReport advance_one_way(const Contract& c,const OneWaySettings& settings,OneWayState& state);
void validate_one_way(const Contract& c,const OneWaySettings& settings,const OneWayState& state);
}
