#include "flow/two_way.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace flow {
namespace {
void check(bool b,const char* text){if(!b)throw std::invalid_argument(text);}
void finite(powder::Vec3 v){check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z),"nonfinite coupling vector");}
double magnitude(powder::Vec3 v){return std::hypot(v.x,v.y,v.z);}
std::vector<KernelEntry> weights(int nx,int ny,double dx,double dy,double ox,double oy,powder::Vec3 position,double h){
    std::vector<KernelEntry> result;double sum=0;
    for(int j=0;j<ny;++j)for(int i=0;i<nx;++i){const double w=std::max(0.,1-std::abs((i+ox)*dx-position.x)/h)*std::max(0.,1-std::abs((j+oy)*dy-position.y)/h);if(w>0){result.push_back({static_cast<std::size_t>(j)*nx+i,w});sum+=w;}}
    check(std::isfinite(sum)&&sum>0,"empty kernel support");for(auto& entry:result)entry.weight/=sum;return result;
}
double gather(const std::vector<double>& f,const std::vector<KernelEntry>& stencil){double result=0;for(const auto& entry:stencil){check(entry.index<f.size()&&std::isfinite(entry.weight)&&entry.weight>=0,"invalid stencil");result+=entry.weight*f[entry.index];}check(std::isfinite(result),"nonfinite gathered field");return result;}
double squared_weights(const std::vector<KernelEntry>& s){double result=0;for(auto a:s)result+=a.weight*a.weight;return result;}
}
ParticleStencil particle_stencil(const Contract& c,powder::Vec3 position,double h){
    const auto d=validate(c);finite(position);check(position.z==0,"two-way model requires z=0 plane");
    check(std::isfinite(h)&&h>=2*std::max(d.dx_m,d.dy_m)&&h<c.grid.height_m/2,"kernel radius must span at least two cells and fit inside channel");
    const double margin=h+std::max(d.dx_m,d.dy_m);
    check(position.x>=margin&&position.x<=c.grid.length_m-margin&&position.y>=margin&&position.y<=c.grid.height_m-margin,"kernel reaches boundary guard: no truncated or renormalized boundary forcing");
    return {weights(c.grid.nx,c.grid.ny,d.dx_m,d.dy_m,.5,.5,position,h),weights(c.grid.nx+1,c.grid.ny,d.dx_m,d.dy_m,0,.5,position,h),weights(c.grid.nx,c.grid.ny+1,d.dx_m,d.dy_m,.5,0,position,h)};
}
VolumeFraction map_particle_volume(const Contract& c,const std::vector<powder::Particle>& particles,double h){
    const auto d=validate(c);powder::validate_particles(particles);VolumeFraction result;result.solid_fraction.assign(Layout(c).pressure_count(),0);
    auto ordered=particles;std::sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    const double cell=d.dx_m*d.dy_m*c.grid.reporting_depth_m;
    for(const auto& p:ordered){const auto stencil=particle_stencil(c,p.position_m,h);const double volume=powder::sphere_properties(p.diameter_m,p.material).mass_kg/p.material.density_kg_m3;result.particle_volume_m3+=volume;
        for(auto entry:stencil.p)result.solid_fraction[entry.index]+=volume*entry.weight/cell;
    }
    for(double alpha:result.solid_fraction){check(std::isfinite(alpha)&&alpha>=0,"invalid mapped volume fraction");result.max_fraction=std::max(result.max_fraction,alpha);result.grid_volume_m3+=alpha*cell;}
    check(std::abs(result.grid_volume_m3-result.particle_volume_m3)<=1e-12*std::max(result.particle_volume_m3,1e-30),"mapped particle volume not conserved");return result;
}
void validate_dilute_volume(const VolumeFraction& volume){for(double alpha:volume.solid_fraction)check(std::isfinite(alpha)&&alpha>=0&&alpha<=.001,"solid fraction exceeds dilute envelope 0.001; no clipping, dense model required");}
powder::Vec3 gather_velocity(const Fields& air,const ParticleStencil& stencil){return {gather(air.u_m_s,stencil.u),gather(air.v_m_s,stencil.v),0};}
void scatter_reaction(MomentumSource& source,const ParticleStencil& stencil,powder::Vec3 force){
    finite(force);check(force.z==0,"3D reaction unsupported");
    for(auto entry:stencil.u){check(entry.index<source.u_force_n.size(),"reaction u storage mismatch");source.u_force_n[entry.index]-=entry.weight*force.x;}
    for(auto entry:stencil.v){check(entry.index<source.v_force_n.size(),"reaction v storage mismatch");source.v_force_n[entry.index]-=entry.weight*force.y;}
}
void validate_two_way(const Contract& c,const TwoWaySettings& settings,const TwoWayState& state){
    const auto d=validate(c);validate_fields(c,state.air);powder::validate_particles(state.particles);finite(settings.gravity_m_s2);
    check(settings.gravity_m_s2.z==0&&magnitude(settings.gravity_m_s2)<=9.81,"invalid planar gravity");
    check(settings.coupling_substeps>=1&&settings.coupling_substeps<=10000,"coupling substeps outside 1..10000");
    check(state.particles.size()<=32,"two-way baseline supports at most 32 isolated spheres");
    // Validate kernel even for the empty-particle limit.
    particle_stencil(c,{c.grid.length_m/2,c.grid.height_m/2,0},settings.kernel_radius_m);
    for(const auto& p:state.particles){check(p.velocity_m_s.z==0,"particle leaves 2D plane");check(p.diameter_m<=.1*std::min(d.dx_m,d.dy_m),"diameter exceeds 0.1 cell");
        check(std::min(p.position_m.y,c.grid.height_m-p.position_m.y)>=50*p.diameter_m,"wall correction required");
        const auto stencil=particle_stencil(c,p.position_m,settings.kernel_radius_m);const auto drag=stokes_drag(c,p,gather_velocity(state.air,stencil));
        const double mass=powder::sphere_properties(p.diameter_m,p.material).mass_kg;
        const double gas_mass=d.rho_kg_m3*d.dx_m*d.dy_m*c.grid.reporting_depth_m;
        const double rate=(1+(settings.reaction_enabled?mass/gas_mass*std::max(squared_weights(stencil.u),squared_weights(stencil.v)):0))/drag.relaxation_time_s;
        // Number-of-particles factor conservatively bounds overlapping kernels.
        check(c.dt_s/settings.coupling_substeps*rate*static_cast<double>(std::max<std::size_t>(1,state.particles.size()))<=.05,"coupling step exceeds drag exchange time bound");
    }
    for(std::size_t i=0;i<state.particles.size();++i)for(std::size_t j=0;j<i;++j){const auto& a=state.particles[i];const auto& b=state.particles[j];check(std::hypot(a.position_m.x-b.position_m.x,a.position_m.y-b.position_m.y)>=50*std::max(a.diameter_m,b.diameter_m),"particle interactions outside isolated-sphere envelope");}
    validate_dilute_volume(map_particle_volume(c,state.particles,settings.kernel_radius_m));
}
TwoWayReport advance_two_way(const Contract& c,const TwoWaySettings& settings,TwoWayState& state){
    validate_two_way(c,settings,state);const auto d=validate(c);check(state.air.accepted_steps<d.steps,"simulation duration already reached");
    Contract fine=c;fine.dt_s=c.dt_s/settings.coupling_substeps;validate(fine);
    auto next=state;next.air.accepted_steps*=static_cast<std::uint64_t>(settings.coupling_substeps);validate_fields(fine,next.air);
    TwoWayReport report;const auto l=Layout(c);const double gas_mass=d.rho_kg_m3*d.dx_m*d.dy_m*c.grid.reporting_depth_m;
    // Canonical IDs make scatter accumulation independent of caller ordering.
    std::sort(next.particles.begin(),next.particles.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    for(int k=0;k<settings.coupling_substeps;++k){
        MomentumSource source;source.u_force_n.assign(l.u_count(),0);source.v_force_n.assign(l.v_count(),0);
        auto particles=next.particles;powder::Vec3 particle_impulse;
        for(auto& p:particles){const auto stencil=particle_stencil(fine,p.position_m,settings.kernel_radius_m);const auto drag=stokes_drag(fine,p,gather_velocity(next.air,stencil));
            const double mass=powder::sphere_properties(p.diameter_m,p.material).mass_kg;
            const double rate=(1+(settings.reaction_enabled?mass/gas_mass*std::max(squared_weights(stencil.u),squared_weights(stencil.v)):0))/drag.relaxation_time_s;
            report.max_dt_over_exchange_time=std::max(report.max_dt_over_exchange_time,fine.dt_s*rate*static_cast<double>(std::max<std::size_t>(1,particles.size())));
            report.max_particle_reynolds=std::max(report.max_particle_reynolds,drag.reynolds);
            particle_impulse.x+=fine.dt_s*drag.force_n.x;particle_impulse.y+=fine.dt_s*drag.force_n.y;
            if(settings.reaction_enabled)scatter_reaction(source,stencil,drag.force_n);
            powder::ForceAccumulator force;force.add(drag.force_n);force.add_gravity(mass,settings.gravity_m_s2);const auto before=p.position_m;powder::integrate_translation(p,force,fine.dt_s);
            check(std::hypot(p.position_m.x-before.x,p.position_m.y-before.y)<=.1*std::min(d.dx_m,d.dy_m),"particle substep displacement exceeds 0.1 cell");
        }
        report.air=settings.reaction_enabled?advance_with_source(fine,next.air,source):advance(fine,next.air);
        report.particle_drag_impulse_n_s.x+=particle_impulse.x;report.particle_drag_impulse_n_s.y+=particle_impulse.y;
        report.gas_source_impulse_n_s.x+=report.air.applied_source_impulse_n_s[0];report.gas_source_impulse_n_s.y+=report.air.applied_source_impulse_n_s[1];
        if(settings.reaction_enabled){const double error=std::hypot(particle_impulse.x+report.air.applied_source_impulse_n_s[0],particle_impulse.y+report.air.applied_source_impulse_n_s[1]);report.exchange_residual_n_s=std::max(report.exchange_residual_n_s,error);check(error<=1e-20+1e-9*magnitude(particle_impulse),"actual gas source and particle drag impulse do not balance");}
        next.particles=std::move(particles);auto per_step=settings;per_step.coupling_substeps=1;validate_two_way(fine,per_step,next);
    }
    report.volume=map_particle_volume(c,next.particles,settings.kernel_radius_m);
    next.air.accepted_steps=state.air.accepted_steps+1;next.air.physical_time_s=static_cast<double>(next.air.accepted_steps)*c.dt_s;validate_two_way(c,settings,next);
    state=std::move(next);return report;
}
}
