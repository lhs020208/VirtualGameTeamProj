#include "flow/one_way.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <functional>

namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void reject(const std::function<void()>& f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,"invalid case accepted");}
powder::Particle particle(){powder::Particle p;p.id=7;p.diameter_m=30e-6;p.material.density_kg_m3=1000;p.position_m={.05,.005,0};return p;}
flow::Contract still(){flow::Contract c;c.boundaries[0].value=c.boundaries[1].value;c.duration_s=.02;return c;}
}
int main(int argc,char** argv){try{
    std::ofstream file;std::ostream* stream=&std::cout;if(argc==2){file.open(argv[1]);check(bool(file),"cannot write metrics");stream=&file;}auto& out=*stream;out<<std::setprecision(17)<<"case,value,auxiliary\n";
    auto c=still();auto p=particle();auto d=flow::validate(c);const auto drag=flow::stokes_drag(c,p,{.02,0,0});
    const double tau=p.material.density_kg_m3*p.diameter_m*p.diameter_m/(18*flow::ViscosityPaS);
    check(std::abs(drag.relaxation_time_s/tau-1)<1e-14,"relaxation formula");check(drag.force_n.x>0,"drag sign");
    p.velocity_m_s={.02,0,0};check(flow::stokes_drag(c,p,{.02,0,0}).force_n.x==0,"zero slip");p=particle();
    auto f=flow::make_initial_fields(c);flow::Layout layout(c);
    for(int j=0;j<c.grid.ny;++j)for(int i=0;i<=c.grid.nx;++i){auto xy=layout.u_position(i,j);f.u_m_s[layout.u_index(i,j)]=.01+.02*xy[0]+.1*xy[1];}
    for(int j=1;j<c.grid.ny;++j)for(int i=0;i<c.grid.nx;++i){auto xy=layout.v_position(i,j);f.v_m_s[layout.v_index(i,j)]=.001+.01*xy[0]-.02*xy[1];}
    const powder::Vec3 position{.0371,.00431,0};auto sampled=flow::sample_velocity(c,f,position);
    check(std::abs(sampled.x-(.01+.02*position.x+.1*position.y))<1e-16&&std::abs(sampled.y-(.001+.01*position.x-.02*position.y))<1e-16,"staggered affine interpolation");
    out<<"affine_interpolation_error,"<<std::abs(sampled.x-(.01+.02*position.x+.1*position.y))<<",0\n";
    double old_spatial=0;
    for(int ny:{8,16,32}){auto grid=c;grid.grid.nx=4*ny;grid.grid.ny=ny;auto field=flow::make_initial_fields(grid);flow::Layout l(grid);
        for(int j=0;j<ny;++j)for(int i=0;i<=grid.grid.nx;++i){const double y=l.u_position(i,j)[1]/grid.grid.height_m;field.u_m_s[l.u_index(i,j)]=.12*y*(1-y);}
        const auto value=flow::sample_velocity(grid,field,{.05,grid.grid.height_m/3,0});const double error=std::abs(value.x-.12*(1./3)*(2./3));
        if(old_spatial)check(std::abs(old_spatial/error-4)<1e-10,"parabolic interpolation spatial convergence");old_spatial=error;out<<"interpolation_grid_"<<ny<<','<<error<<",0\n";
    }
    reject([&]{flow::sample_velocity(c,f,{0,.005,0});});reject([&]{flow::sample_velocity(c,f,{.05,.005,.001});});
    auto high=p;high.velocity_m_s={1,0,0};reject([&]{flow::stokes_drag(c,high,{});});
    auto small=p;small.diameter_m=1e-6;reject([&]{flow::stokes_drag(c,small,{});});
    auto light=p;light.material.density_kg_m3=10;reject([&]{flow::stokes_drag(c,light,{});});
    auto spin=p;spin.angular_velocity_rad_s.z=1;reject([&]{flow::stokes_drag(c,spin,{});});
    double previous_v=0,previous_x=0;
    for(int subdivisions:{20,40,80}){auto q=p;const double dt=tau/subdivisions;const int steps=5*subdivisions;const double time=dt*steps;
        for(int k=0;k<steps;++k){powder::ForceAccumulator forces;forces.add(flow::stokes_drag(c,q,{.02,0,0}).force_n);powder::integrate_translation(q,forces,dt);}
        const double reference_v=.02*(1-std::exp(-time/tau));const double reference_x=p.position_m.x+.02*(time-tau*(1-std::exp(-time/tau)));
        const double ev=std::abs(q.velocity_m_s.x-reference_v),ex=std::abs(q.position_m.x-reference_x);
        if(previous_v){check(previous_v/ev>1.8&&previous_v/ev<2.2,"relaxation velocity time convergence");check(previous_x/ex>1.8&&previous_x/ex<2.2,"relaxation position time convergence");}
        out<<"relaxation_"<<subdivisions<<','<<ev<<','<<ex<<'\n';previous_v=ev;previous_x=ex;
    }
    double previous=0;
    for(int substeps:{5,10,20}){flow::OneWaySettings settings;settings.particle_substeps=substeps;flow::OneWayState state{flow::make_initial_fields(c),{p}};const auto initial=state.air;
        for(std::uint64_t k=0;k<d.steps;++k)flow::advance_one_way(c,settings,state);
        const double reference=-9.81*tau*(1-std::exp(-c.duration_s/tau));const double error=std::abs(state.particles[0].velocity_m_s.y-reference);
        if(previous)check(previous/error>1.8&&previous/error<2.2,"settling time convergence");previous=error;
        check(state.air.u_m_s==initial.u_m_s&&state.air.v_m_s==initial.v_m_s&&state.air.pressure_gauge_pa==initial.pressure_gauge_pa,"particle reacted onto still air");
        check(std::abs(state.particles[0].velocity_m_s.y/(-9.81*tau)-1)<.001,"terminal velocity");out<<"settling_"<<substeps<<','<<error<<','<<state.particles[0].velocity_m_s.y<<'\n';
    }
    flow::Contract driven;driven.duration_s=.005;flow::OneWaySettings settings;settings.gravity_m_s2={};
    flow::OneWayState state{flow::make_initial_fields(driven),{p}},control{state.air,{}},off=state;auto no_drag=settings;no_drag.drag_enabled=false;
    auto repeat=state;
    for(int k=0;k<10;++k){flow::advance_one_way(driven,settings,state);flow::advance_one_way(driven,settings,control);flow::advance_one_way(driven,no_drag,off);flow::advance_one_way(driven,settings,repeat);}
    check(state.particles[0].velocity_m_s.x>0&&off.particles[0].velocity_m_s.x==0,"computed air did not drive particle");
    check(state.air.u_m_s==control.air.u_m_s&&state.air.pressure_gauge_pa==control.air.pressure_gauge_pa&&state.air.v_m_s==control.air.v_m_s,"air depends on particles");
    check(state.particles[0].position_m.x==repeat.particles[0].position_m.x&&state.particles[0].velocity_m_s.x==repeat.particles[0].velocity_m_s.x,"one-way repeatability");
    out<<"computed_air_particle_velocity,"<<state.particles[0].velocity_m_s.x<<",0\n";
    double velocities[3]{};int time_index=0;
    for(double step:{.0005,.00025,.000125}){auto coupled=driven;coupled.dt_s=step;coupled.duration_s=.01;flow::OneWayState test{flow::make_initial_fields(coupled),{p}};powder::Vec3 impulse;
        for(std::uint64_t k=0;k<flow::validate(coupled).steps;++k){const auto r=flow::advance_one_way(coupled,settings,test);impulse.x+=r.drag_impulse_n_s.x;impulse.y+=r.drag_impulse_n_s.y;}
        const double mass=powder::sphere_properties(p.diameter_m,p.material).mass_kg;
        check(std::abs(impulse.x-mass*test.particles[0].velocity_m_s.x)<1e-25,"drag impulse and particle momentum disagree");velocities[time_index++]=test.particles[0].velocity_m_s.x;
    }
    const double time_ratio=std::abs((velocities[0]-velocities[1])/(velocities[1]-velocities[2]));check(time_ratio>1.8&&time_ratio<2.2,"coupled air/particle time convergence");out<<"coupled_time_self_convergence,"<<time_ratio<<",0\n";
    // Roll back a failure discovered after the air has advanced, during the
    // particle trajectory: an outward-moving particle crosses stencil guard.
    auto boundary=p;boundary.position_m.x=d.dx_m/2+1e-7;boundary.velocity_m_s.x=-.01;
    flow::OneWayState bad{flow::make_initial_fields(c),{boundary}};auto saved=bad;
    reject([&]{flow::advance_one_way(c,settings,bad);});check(bad.air.accepted_steps==0&&bad.air.u_m_s==saved.air.u_m_s&&bad.particles[0].position_m.x==saved.particles[0].position_m.x,"coupled rollback failed");
    auto large=p;large.diameter_m=.04;bad={flow::make_initial_fields(c),{large}};reject([&]{flow::validate_one_way(c,settings,bad);});
    bad={flow::make_initial_fields(c),{p}};auto coarse=settings;coarse.particle_substeps=1;reject([&]{flow::validate_one_way(c,coarse,bad);});
    bad.particles.push_back(p);bad.particles.back().id=8;reject([&]{flow::validate_one_way(c,settings,bad);});
    out<<"rollback_and_scope_guards,1,0\n";std::cout<<"PASS one-way drag verification\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
