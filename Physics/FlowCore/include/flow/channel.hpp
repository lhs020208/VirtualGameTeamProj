#pragma once
#include "flow/two_way.hpp"

namespace flow {
inline constexpr const char* ChannelVersion="0.5.0-buffered-channel";
struct Release {
    powder::ParticleId id=1;
    double requested_time_s=0,diameter_m=30e-6,density_kg_m3=1000;
    double y_m=.005,velocity_x_m_s=.02875;
};
struct ChannelSettings {
    TwoWaySettings coupling;
    double inlet_x_m=.008,outlet_x_m=.012;
    std::vector<Release> releases{{}};
};
enum class TransitStatus {Pending,Active,Exited};
struct Transit {
    Release request;
    TransitStatus status=TransitStatus::Pending;
    double mass_kg=0,entry_time_s=-1,crossing_time_s=-1,removal_time_s=-1;
    powder::Particle last_particle;
};
struct ChannelState {
    TwoWayState coupled;
    std::vector<Transit> records;
    double air_in_mass_kg=0,air_out_mass_kg=0;
};
struct ChannelBalance {
    std::size_t pending=0,due_waiting=0,active=0,exited=0;
    double requested_mass_kg=0,pending_mass_kg=0,introduced_mass_kg=0,active_mass_kg=0,exited_mass_kg=0;
    double particle_mass_residual_kg=0,air_mass_residual_kg=0;
    double mapped_volume_m3=0,test_volume_m3=0,buffer_volume_m3=0,max_solid_fraction=0;
};
struct ChannelReport {
    ChannelBalance balance;
    double probe_inlet_gauge_pa=0,probe_outlet_gauge_pa=0;
    double max_air_divergence_s_inv=0,max_air_mass_relative=0,max_exchange_residual_n_s=0;
    powder::Vec3 gas_impulse_n_s,test_reaction_impulse_n_s,buffer_reaction_impulse_n_s;
    std::size_t introduced_this_step=0,exited_this_step=0;
};
ChannelState make_channel(const Contract& c,const ChannelSettings& settings);
ChannelBalance channel_balance(const Contract& c,const ChannelSettings& settings,const ChannelState& state);
double section_pressure(const Contract& c,const Fields& fields,double x_m);
// Virtual particle ports lie inside a padded gas domain. Every kernel remains
// interior; no force truncation or silent boundary renormalization occurs.
// Entire outer step, including queue/events/cumulative balances, is atomic.
ChannelReport advance_channel(const Contract& c,const ChannelSettings& settings,ChannelState& state);
}
