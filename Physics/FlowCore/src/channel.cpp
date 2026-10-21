#include "flow/channel.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

namespace flow {
namespace {
void check(bool b,const char* message){if(!b)throw std::invalid_argument(message);}
double mass(const Release& r){return powder::sphere_properties(r.diameter_m,{r.density_kg_m3}).mass_kg;}
powder::Particle make_particle(const Release& r,double x){powder::Particle p;p.id=r.id;p.diameter_m=r.diameter_m;p.material.density_kg_m3=r.density_kg_m3;p.position_m={x,r.y_m,0};p.velocity_m_s={r.velocity_x_m_s,0,0};return p;}
void settings_valid(const Contract& c,const ChannelSettings& settings){
    const auto d=validate(c);const double guard=settings.coupling.kernel_radius_m+std::max(d.dx_m,d.dy_m)+.1*std::min(d.dx_m,d.dy_m);
    check(std::isfinite(settings.inlet_x_m)&&std::isfinite(settings.outlet_x_m)&&settings.inlet_x_m>=guard&&settings.outlet_x_m<=c.grid.length_m-guard&&settings.outlet_x_m>settings.inlet_x_m,"particle ports require interior gas buffers and positive test length");
    check(settings.releases.size()<=256,"at most 256 scheduled releases in channel baseline");std::set<powder::ParticleId> ids;
    for(const auto& r:settings.releases){check(ids.insert(r.id).second,"duplicate release ID");check(std::isfinite(r.requested_time_s)&&r.requested_time_s>=0&&r.requested_time_s<c.duration_s,"release time must lie inside run duration");
        auto p=make_particle(r,settings.inlet_x_m);powder::validate(p);check(r.velocity_x_m_s>=0,"reverse injection not supported");
        check(r.diameter_m<=.1*std::min(d.dx_m,d.dy_m),"release diameter exceeds point-particle grid envelope");
        particle_stencil(c,p.position_m,settings.coupling.kernel_radius_m);particle_stencil(c,{settings.outlet_x_m,r.y_m,0},settings.coupling.kernel_radius_m);
    }
}
bool same(const Release& a,const Release& b){return a.id==b.id&&a.requested_time_s==b.requested_time_s&&a.diameter_m==b.diameter_m&&a.density_kg_m3==b.density_kg_m3&&a.y_m==b.y_m&&a.velocity_x_m_s==b.velocity_x_m_s;}
double overlap_fraction(double center,double width,double left,double right){return std::max(0.,std::min(center+width/2,right)-std::max(center-width/2,left))/width;}
Transit& record(ChannelState& state,powder::ParticleId id){auto it=std::find_if(state.records.begin(),state.records.end(),[&](const auto& r){return r.request.id==id;});check(it!=state.records.end(),"missing transit record");return *it;}
}
ChannelState make_channel(const Contract& c,const ChannelSettings& settings){
    settings_valid(c,settings);ChannelState state;state.coupled.air=make_initial_fields(c);validate_two_way(c,settings.coupling,state.coupled);
    auto requests=settings.releases;std::sort(requests.begin(),requests.end(),[](const auto& a,const auto& b){return a.requested_time_s==b.requested_time_s?a.id<b.id:a.requested_time_s<b.requested_time_s;});
    for(const auto& r:requests){auto particle=make_particle(r,settings.inlet_x_m);auto probe=state.coupled;probe.particles={particle};validate_two_way(c,settings.coupling,probe);Transit t;t.request=r;t.mass_kg=mass(r);t.last_particle=particle;state.records.push_back(t);}
    channel_balance(c,settings,state);return state;
}
ChannelBalance channel_balance(const Contract& c,const ChannelSettings& settings,const ChannelState& state){
    settings_valid(c,settings);validate_two_way(c,settings.coupling,state.coupled);check(state.records.size()==settings.releases.size(),"release ledger shape changed");
    std::map<powder::ParticleId,const Release*> requests;for(const auto& r:settings.releases)requests.emplace(r.id,&r);
    std::map<powder::ParticleId,const powder::Particle*> active;for(const auto& p:state.coupled.particles){active.emplace(p.id,&p);check(p.position_m.x>=settings.inlet_x_m&&p.position_m.x<settings.outlet_x_m,"active particle outside center-count test region");}
    ChannelBalance result;std::set<powder::ParticleId> seen;
    double previous_time=-1;powder::ParticleId previous_id=0;
    for(const auto& t:state.records){auto it=requests.find(t.request.id);check(it!=requests.end()&&same(*it->second,t.request)&&seen.insert(t.request.id).second,"release configuration changed during run");
        check(t.request.requested_time_s>previous_time||(t.request.requested_time_s==previous_time&&t.request.id>previous_id),"noncanonical release ledger");previous_time=t.request.requested_time_s;previous_id=t.request.id;
        check(t.mass_kg==mass(t.request),"transit mass changed");result.requested_mass_kg+=t.mass_kg;
        if(t.status==TransitStatus::Pending){check(t.entry_time_s==-1&&t.crossing_time_s==-1&&t.removal_time_s==-1&&!active.count(t.request.id),"invalid pending record");++result.pending;result.pending_mass_kg+=t.mass_kg;if(t.request.requested_time_s<=state.coupled.air.physical_time_s)++result.due_waiting;}
        else if(t.status==TransitStatus::Active){check(active.count(t.request.id)&&std::isfinite(t.entry_time_s)&&t.entry_time_s>=t.request.requested_time_s&&t.entry_time_s<=state.coupled.air.physical_time_s&&t.crossing_time_s==-1&&t.removal_time_s==-1,"invalid active record");const auto& p=*active.at(t.request.id);check(p.diameter_m==t.request.diameter_m&&p.material.density_kg_m3==t.request.density_kg_m3,"active particle properties changed");++result.active;result.active_mass_kg+=t.mass_kg;result.introduced_mass_kg+=t.mass_kg;}
        else if(t.status==TransitStatus::Exited){check(!active.count(t.request.id)&&std::isfinite(t.entry_time_s)&&std::isfinite(t.crossing_time_s)&&std::isfinite(t.removal_time_s)&&t.entry_time_s>=t.request.requested_time_s&&t.crossing_time_s>=t.entry_time_s&&t.removal_time_s>=t.crossing_time_s&&t.removal_time_s<=state.coupled.air.physical_time_s+1e-12,"invalid exit record");++result.exited;result.exited_mass_kg+=t.mass_kg;result.introduced_mass_kg+=t.mass_kg;}
        else throw std::invalid_argument("unknown transit status");
    }
    check(result.active==active.size(),"untracked active particle");
    result.particle_mass_residual_kg=std::max(std::abs(result.requested_mass_kg-result.pending_mass_kg-result.active_mass_kg-result.exited_mass_kg),std::abs(result.introduced_mass_kg-result.active_mass_kg-result.exited_mass_kg));
    check(result.particle_mass_residual_kg<=1e-12*std::max(result.requested_mass_kg,1e-30),"particle mass ledger imbalance");
    check(std::isfinite(state.air_in_mass_kg)&&std::isfinite(state.air_out_mass_kg)&&state.air_in_mass_kg>=0&&state.air_out_mass_kg>=0,"invalid cumulative gas mass");
    result.air_mass_residual_kg=std::abs(state.air_in_mass_kg-state.air_out_mass_kg);check(result.air_mass_residual_kg<=1e-15+c.projection.mass_balance_relative*std::max(state.air_in_mass_kg,state.air_out_mass_kg),"integrated gas balance failed");
    const auto volume=map_particle_volume(c,state.coupled.particles,settings.coupling.kernel_radius_m);result.mapped_volume_m3=volume.grid_volume_m3;result.max_solid_fraction=volume.max_fraction;const auto d=validate(c);Layout layout(c);
    for(int j=0;j<c.grid.ny;++j)for(int i=0;i<c.grid.nx;++i){const double v=volume.solid_fraction[layout.p_index(i,j)]*d.dx_m*d.dy_m*c.grid.reporting_depth_m;const double f=overlap_fraction((i+.5)*d.dx_m,d.dx_m,settings.inlet_x_m,settings.outlet_x_m);result.test_volume_m3+=f*v;result.buffer_volume_m3+=(1-f)*v;}
    check(std::abs(result.test_volume_m3+result.buffer_volume_m3-result.mapped_volume_m3)<=1e-12*std::max(result.mapped_volume_m3,1e-30),"test/buffer volume partition failed");return result;
}
double section_pressure(const Contract& c,const Fields& fields,double x){
    validate_fields(c,fields);const auto d=validate(c);check(std::isfinite(x)&&x>=d.dx_m/2&&x<=c.grid.length_m-d.dx_m/2,"pressure section outside interior");
    const double cell=x/d.dx_m-.5;int i=static_cast<int>(std::floor(cell));if(i==c.grid.nx-1)--i;const double fraction=cell-i;Layout layout(c);double sum=0;
    for(int j=0;j<c.grid.ny;++j)sum+=(1-fraction)*fields.pressure_gauge_pa[layout.p_index(i,j)]+fraction*fields.pressure_gauge_pa[layout.p_index(i+1,j)];return sum/c.grid.ny;
}
ChannelReport advance_channel(const Contract& c,const ChannelSettings& settings,ChannelState& state){
    channel_balance(c,settings,state);const auto d=validate(c);check(state.coupled.air.accepted_steps<d.steps,"channel duration complete");auto next=state;
    Contract fine=c;fine.dt_s=c.dt_s/settings.coupling.coupling_substeps;validate(fine);auto one=settings;one.coupling.coupling_substeps=1;next.coupled.air.accepted_steps*=static_cast<std::uint64_t>(settings.coupling.coupling_substeps);
    ChannelReport report;Layout layout(c);
    for(int step=0;step<settings.coupling.coupling_substeps;++step){
        const double start_time=next.coupled.air.physical_time_s;
        for(auto& t:next.records){if(t.status!=TransitStatus::Pending)continue;if(t.request.requested_time_s>start_time)break;
            auto p=make_particle(t.request,settings.inlet_x_m);bool blocked=next.coupled.particles.size()>=32;
            for(const auto& q:next.coupled.particles)if(std::hypot(p.position_m.x-q.position_m.x,p.position_m.y-q.position_m.y)<55*std::max(p.diameter_m,q.diameter_m))blocked=true;
            if(blocked)break; // FIFO; no silent dropping or geometric overlap.
            auto proposal=next.coupled;proposal.particles.push_back(p);validate_two_way(fine,one.coupling,proposal);next.coupled=std::move(proposal);t.status=TransitStatus::Active;t.entry_time_s=start_time;t.last_particle=p;++report.introduced_this_step;
        }
        auto old_particles=next.coupled.particles;
        if(settings.coupling.reaction_enabled){MomentumSource source;source.u_force_n.assign(layout.u_count(),0);source.v_force_n.assign(layout.v_count(),0);
            auto ordered=old_particles;std::sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.id<b.id;});
            for(const auto& p:ordered){auto stencil=particle_stencil(fine,p.position_m,settings.coupling.kernel_radius_m);scatter_reaction(source,stencil,stokes_drag(fine,p,gather_velocity(next.coupled.air,stencil)).force_n);}
            for(int j=0;j<c.grid.ny;++j)for(int i=0;i<=c.grid.nx;++i){double impulse=source.u_force_n[layout.u_index(i,j)]*fine.dt_s;double f=overlap_fraction(i*d.dx_m,d.dx_m,settings.inlet_x_m,settings.outlet_x_m);report.test_reaction_impulse_n_s.x+=f*impulse;report.buffer_reaction_impulse_n_s.x+=(1-f)*impulse;}
            for(int j=0;j<=c.grid.ny;++j)for(int i=0;i<c.grid.nx;++i){double impulse=source.v_force_n[layout.v_index(i,j)]*fine.dt_s;double f=overlap_fraction((i+.5)*d.dx_m,d.dx_m,settings.inlet_x_m,settings.outlet_x_m);report.test_reaction_impulse_n_s.y+=f*impulse;report.buffer_reaction_impulse_n_s.y+=(1-f)*impulse;}
        }
        const auto exchange=advance_two_way(fine,one.coupling,next.coupled);
        report.max_air_divergence_s_inv=std::max(report.max_air_divergence_s_inv,exchange.air.divergence_linf_s_inv);report.max_air_mass_relative=std::max(report.max_air_mass_relative,exchange.air.mass_balance_relative);report.max_exchange_residual_n_s=std::max(report.max_exchange_residual_n_s,exchange.exchange_residual_n_s);
        report.gas_impulse_n_s.x+=exchange.gas_source_impulse_n_s.x;report.gas_impulse_n_s.y+=exchange.gas_source_impulse_n_s.y;
        next.air_in_mass_kg+=exchange.air.inlet_mass_kg_s*fine.dt_s;next.air_out_mass_kg+=exchange.air.outlet_mass_kg_s*fine.dt_s;
        std::vector<powder::Particle> remaining;
        for(const auto& p:next.coupled.particles){auto& t=record(next,p.id);t.last_particle=p;check(p.position_m.x>=settings.inlet_x_m,"particle reversed through supply port");
            if(p.position_m.x>=settings.outlet_x_m){auto it=std::find_if(old_particles.begin(),old_particles.end(),[&](const auto& old){return old.id==p.id;});check(it!=old_particles.end()&&p.position_m.x>it->position_m.x,"invalid exit crossing");double fraction=(settings.outlet_x_m-it->position_m.x)/(p.position_m.x-it->position_m.x);check(fraction>=0&&fraction<=1,"crossing outside substep");t.status=TransitStatus::Exited;t.crossing_time_s=start_time+fraction*fine.dt_s;t.removal_time_s=next.coupled.air.physical_time_s;++report.exited_this_step;
            }else remaining.push_back(p);
        }
        next.coupled.particles=std::move(remaining);channel_balance(fine,one,next);
    }
    const double force_partition_error=std::hypot(report.gas_impulse_n_s.x-report.test_reaction_impulse_n_s.x-report.buffer_reaction_impulse_n_s.x,report.gas_impulse_n_s.y-report.test_reaction_impulse_n_s.y-report.buffer_reaction_impulse_n_s.y);
    check(force_partition_error<=1e-20+1e-9*std::hypot(report.gas_impulse_n_s.x,report.gas_impulse_n_s.y),"test/buffer impulse partition failed");
    next.coupled.air.accepted_steps=state.coupled.air.accepted_steps+1;next.coupled.air.physical_time_s=static_cast<double>(next.coupled.air.accepted_steps)*c.dt_s;
    report.balance=channel_balance(c,settings,next);report.probe_inlet_gauge_pa=section_pressure(c,next.coupled.air,settings.inlet_x_m);report.probe_outlet_gauge_pa=section_pressure(c,next.coupled.air,settings.outlet_x_m);state=std::move(next);return report;
}
}

