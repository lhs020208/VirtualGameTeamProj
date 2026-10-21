#pragma once
#include "flow/one_way.hpp"

namespace flow {
inline constexpr const char* TwoWayVersion="0.4.0-dilute-two-way";
struct KernelEntry {std::size_t index;double weight;};
struct ParticleStencil {std::vector<KernelEntry> p,u,v;};
// Fixed physical tent support, normalized positive quadrature weights.
ParticleStencil particle_stencil(const Contract& c,powder::Vec3 position,double radius_m);
struct VolumeFraction {
    std::vector<double> solid_fraction;
    double particle_volume_m3=0,grid_volume_m3=0,max_fraction=0;
};
VolumeFraction map_particle_volume(const Contract& c,const std::vector<powder::Particle>& particles,double kernel_radius_m);
void validate_dilute_volume(const VolumeFraction& volume); // no clipping; alpha<=1e-3
powder::Vec3 gather_velocity(const Fields& air,const ParticleStencil& stencil);
void scatter_reaction(MomentumSource& source,const ParticleStencil& stencil,powder::Vec3 particle_force_n);
struct TwoWaySettings {
    int coupling_substeps=20;
    double kernel_radius_m=.0025;
    powder::Vec3 gravity_m_s2{};
    bool reaction_enabled=true; // false is a matched one-way numerical control
};
using TwoWayState=OneWayState;
struct TwoWayReport {
    StepReport air;
    VolumeFraction volume;
    powder::Vec3 particle_drag_impulse_n_s,gas_source_impulse_n_s;
    double exchange_residual_n_s=0,max_particle_reynolds=0,max_dt_over_exchange_time=0;
};
// Dilute point-force approximation: volume fractions are conservative
// diagnostics/guards, not finite-porosity mass or momentum equations.
TwoWayReport advance_two_way(const Contract& c,const TwoWaySettings& settings,TwoWayState& state);
void validate_two_way(const Contract& c,const TwoWaySettings& settings,const TwoWayState& state);
}
