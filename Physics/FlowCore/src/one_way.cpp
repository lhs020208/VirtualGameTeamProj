#include "flow/one_way.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace flow {
namespace {
void check(bool ok,const char* reason){if(!ok)throw std::invalid_argument(reason);}
void finite(powder::Vec3 p){check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"nonfinite coupling vector");}
double size(powder::Vec3 p){return std::hypot(p.x,p.y,p.z);}
powder::Vec3 sample_unchecked(const Contract& c,const Fields& f,powder::Vec3 position){
    finite(position);const double dx=c.grid.length_m/c.grid.nx,dy=c.grid.height_m/c.grid.ny;
    check(position.z==0,"one-way model requires the z=0 plane");
    check(position.x>=dx/2&&position.x<=c.grid.length_m-dx/2&&position.y>=dy/2&&position.y<=c.grid.height_m-dy/2,"particle outside interior interpolation stencil; no clamping");
    auto interpolate=[](const std::vector<double>& a,int width,int height,double x,double y){
        int i=static_cast<int>(std::floor(x)),j=static_cast<int>(std::floor(y));
        // Exact upper node belongs to the final valid interpolation interval.
        if(i==width-1)--i;if(j==height-1)--j;
        check(i>=0&&j>=0&&i+1<width&&j+1<height,"invalid interpolation stencil");
        const double tx=x-i,ty=y-j;
        auto at=[&](int ix,int iy){return a[static_cast<std::size_t>(iy)*width+ix];};
        return (1-ty)*((1-tx)*at(i,j)+tx*at(i+1,j))+ty*((1-tx)*at(i,j+1)+tx*at(i+1,j+1));
    };
    return {interpolate(f.u_m_s,c.grid.nx+1,c.grid.ny,position.x/dx,position.y/dy-.5),interpolate(f.v_m_s,c.grid.nx,c.grid.ny+1,position.x/dx-.5,position.y/dy),0};
}
void geometry(const Contract& c,const powder::Particle& p){
    const auto d=validate(c);const double diameter=p.diameter_m;
    check(diameter<=.1*std::min(d.dx_m,d.dy_m),"particle diameter exceeds 0.1 cell width: point-particle model rejected");
    check(p.position_m.y>=50*diameter&&c.grid.height_m-p.position_m.y>=50*diameter,"particle too close to a wall for uncorrected isolated-sphere drag");
}
}
DragResult stokes_drag(const Contract& c,const powder::Particle& p,powder::Vec3 air){
    const auto d=validate(c);powder::validate(p);finite(air);
    check(p.angular_velocity_rad_s.x==0&&p.angular_velocity_rad_s.y==0&&p.angular_velocity_rad_s.z==0,"rotating particle drag torque not implemented");
    check(p.material.density_kg_m3/d.rho_kg_m3>=100,"density ratio below heavy-particle envelope");
    const powder::Vec3 slip{air.x-p.velocity_m_s.x,air.y-p.velocity_m_s.y,air.z-p.velocity_m_s.z};
    DragResult result;result.reynolds=d.rho_kg_m3*p.diameter_m*size(slip)/ViscosityPaS;
    check(std::isfinite(result.reynolds)&&result.reynolds<=.1,"particle relative Reynolds number exceeds Stokes envelope 0.1");
    result.knudsen_estimate=70e-9*(101325/std::min(c.boundaries[1].value,d.estimated_inlet_absolute_pa))/p.diameter_m;
    check(result.knudsen_estimate<=.01,"particle Kn exceeds 0.01; slip correction required");
    const double beta=3*3.14159265358979323846*ViscosityPaS*p.diameter_m;
    result.force_n={beta*slip.x,beta*slip.y,beta*slip.z};finite(result.force_n);
    result.relaxation_time_s=powder::sphere_properties(p.diameter_m,p.material).mass_kg/beta;
    check(std::isfinite(result.relaxation_time_s)&&result.relaxation_time_s>0,"invalid drag relaxation time");return result;
}
powder::Vec3 sample_velocity(const Contract& c,const Fields& f,powder::Vec3 position){validate_fields(c,f);return sample_unchecked(c,f,position);}
void validate_one_way(const Contract& c,const OneWaySettings& settings,const OneWayState& state){
    const auto d=validate(c);validate_fields(c,state.air);powder::validate_particles(state.particles);
    check(settings.particle_substeps>=1&&settings.particle_substeps<=100000,"particle substeps outside 1..100000");finite(settings.gravity_m_s2);
    check(settings.gravity_m_s2.z==0&&size(settings.gravity_m_s2)<=9.81,"gravity must be planar and at most 9.81 m/s2");
    check(state.particles.size()<=32,"task 24 supports at most 32 isolated test particles");
    double mass=0;
    for(const auto& p:state.particles){geometry(c,p);check(p.velocity_m_s.z==0,"particle velocity leaves the z=0 plane");
        auto air=sample_unchecked(c,state.air,p.position_m);const auto drag=stokes_drag(c,p,air);
        if(settings.drag_enabled)check(c.dt_s/settings.particle_substeps<=.05*drag.relaxation_time_s,"particle step exceeds 0.05 drag relaxation time");
        mass+=powder::sphere_properties(p.diameter_m,p.material).mass_kg;
    }
    check(mass/(d.rho_kg_m3*c.grid.length_m*c.grid.height_m*c.grid.reporting_depth_m)<=.001,"global test-slab mass loading exceeds 0.001; one-way assumption rejected");
    for(std::size_t i=0;i<state.particles.size();++i)for(std::size_t j=0;j<i;++j){auto a=state.particles[i],b=state.particles[j];check(std::hypot(a.position_m.x-b.position_m.x,a.position_m.y-b.position_m.y)>=50*std::max(a.diameter_m,b.diameter_m),"test particles too close: hydrodynamic/contact interactions excluded");}
}
OneWayReport advance_one_way(const Contract& c,const OneWaySettings& settings,OneWayState& state){
    validate_one_way(c,settings,state);auto next=state;OneWayReport report;report.air=advance(c,next.air);
    const double dt=c.dt_s/settings.particle_substeps;
    for(int k=0;k<settings.particle_substeps;++k){
        const double alpha=static_cast<double>(k)/settings.particle_substeps;
        for(auto& p:next.particles){
            auto old_air=sample_unchecked(c,state.air,p.position_m),new_air=sample_unchecked(c,next.air,p.position_m);
            const powder::Vec3 air{(1-alpha)*old_air.x+alpha*new_air.x,(1-alpha)*old_air.y+alpha*new_air.y,0};
            const auto drag=stokes_drag(c,p,air);report.max_particle_reynolds=std::max(report.max_particle_reynolds,drag.reynolds);
            powder::ForceAccumulator force;force.add_gravity(powder::sphere_properties(p.diameter_m,p.material).mass_kg,settings.gravity_m_s2);
            if(settings.drag_enabled){force.add(drag.force_n);report.max_dt_over_tau=std::max(report.max_dt_over_tau,dt/drag.relaxation_time_s);report.drag_impulse_n_s.x+=dt*drag.force_n.x;report.drag_impulse_n_s.y+=dt*drag.force_n.y;}
            const auto before=p.position_m;powder::integrate_translation(p,force,dt);geometry(c,p);
            check(std::hypot(p.position_m.x-before.x,p.position_m.y-before.y)<=.1*std::min(c.grid.length_m/c.grid.nx,c.grid.height_m/c.grid.ny),"particle moved more than 0.1 cell per substep");
            const double beta=static_cast<double>(k+1)/settings.particle_substeps;
            old_air=sample_unchecked(c,state.air,p.position_m);new_air=sample_unchecked(c,next.air,p.position_m);
            const auto after=stokes_drag(c,p,{(1-beta)*old_air.x+beta*new_air.x,(1-beta)*old_air.y+beta*new_air.y,0});
            report.max_particle_reynolds=std::max(report.max_particle_reynolds,after.reynolds);
        }
        // Validate separation after every substep, not just the coupling interval.
        validate_one_way(c,settings,next);
    }
    state=std::move(next);return report;
}
}
