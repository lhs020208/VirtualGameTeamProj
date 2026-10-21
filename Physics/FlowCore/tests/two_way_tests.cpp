#include "flow/two_way.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>

namespace {
void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
void rejects(const std::function<void()>& f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,"invalid input accepted");}
flow::Contract channel(int ny=16){flow::Contract c;c.grid.length_m=.02;c.grid.nx=2*ny;c.grid.ny=ny;c.grid.reporting_depth_m=.001;c.boundaries[0].value=101325.0008688;c.initial=flow::InitialCondition::SteadyAnalyticReference;c.duration_s=.002;return c;}
powder::Particle particle(){powder::Particle p;p.id=9;p.diameter_m=30e-6;p.material.density_kg_m3=1000;p.position_m={.01,.005,0};return p;}
double run(int ny,int sub,bool reaction){auto c=channel(ny);flow::TwoWaySettings s;s.coupling_substeps=sub;s.reaction_enabled=reaction;flow::TwoWayState state{flow::make_initial_fields(c),{particle()}};for(std::uint64_t k=0;k<flow::validate(c).steps;++k)flow::advance_two_way(c,s,state);return state.particles[0].velocity_m_s.x;}
}
int main(int argc,char** argv){try{
    std::ofstream file;std::ostream* stream=&std::cout;if(argc==2){file.open(argv[1]);check(bool(file),"cannot open metrics");stream=&file;}auto& out=*stream;out<<std::setprecision(17)<<"case,value,auxiliary\n";
    auto c=channel();auto p=particle();flow::TwoWaySettings s;const auto d=flow::validate(c);flow::Layout l(c);
    auto volume=flow::map_particle_volume(c,{p},s.kernel_radius_m);flow::validate_dilute_volume(volume);
    check(std::abs(volume.grid_volume_m3/volume.particle_volume_m3-1)<1e-12,"volume integral");out<<"volume_relative_error,"<<std::abs(volume.grid_volume_m3/volume.particle_volume_m3-1)<<','<<volume.max_fraction<<'\n';
    auto huge=p;huge.diameter_m=.01;auto dense=flow::map_particle_volume(c,{huge},s.kernel_radius_m);check(dense.max_fraction>1,"dense test not dense");rejects([&]{flow::validate_dilute_volume(dense);});check(dense.max_fraction>1,"dense fraction was clipped");
    // One shared kernel makes gathering and spreading discrete adjoints.
    auto air=flow::make_initial_fields(c);auto stencil=flow::particle_stencil(c,p.position_m,s.kernel_radius_m);
    for(std::size_t k=0;k<air.u_m_s.size();++k)air.u_m_s[k]=.01+static_cast<double>(k%17)*.0001;
    for(std::size_t k=0;k<air.v_m_s.size();++k)air.v_m_s[k]=static_cast<double>(k%13)*.0001;
    flow::MomentumSource source;source.u_force_n.assign(l.u_count(),0);source.v_force_n.assign(l.v_count(),0);powder::Vec3 force{2e-10,-3e-10,0};flow::scatter_reaction(source,stencil,force);
    auto u=flow::gather_velocity(air,stencil);double work=force.x*u.x+force.y*u.y;
    for(std::size_t k=0;k<air.u_m_s.size();++k)work+=source.u_force_n[k]*air.u_m_s[k];for(std::size_t k=0;k<air.v_m_s.size();++k)work+=source.v_force_n[k]*air.v_m_s[k];
    const double balance=std::hypot(std::accumulate(source.u_force_n.begin(),source.u_force_n.end(),0.)+force.x,std::accumulate(source.v_force_n.begin(),source.v_force_n.end(),0.)+force.y);
    check(balance<1e-24&&std::abs(work)<1e-25,"scatter conservation/adjoint identity");out<<"force_balance_N,"<<balance<<",0\nadjoint_power_residual_W,"<<work<<",0\n";
    // Pure isolated drag exchange with frozen stencil has an independent
    // exponential slip decay: rate=beta*(1/mp+sum(w^2)/mg).
    auto exchange_air=flow::make_initial_fields(c);std::fill(exchange_air.u_m_s.begin(),exchange_air.u_m_s.end(),.02);auto q=p;
    const double mass=powder::sphere_properties(p.diameter_m,p.material).mass_kg,gas_mass=d.rho_kg_m3*d.dx_m*d.dy_m*c.grid.reporting_depth_m;
    double ww=0;for(auto a:stencil.u)ww+=a.weight*a.weight;
    const double beta=3*3.14159265358979323846*flow::ViscosityPaS*p.diameter_m,rate=beta*(1/mass+ww/gas_mass),dt=.01/rate;
    double initial_momentum=mass*q.velocity_m_s.x;for(double a:exchange_air.u_m_s)initial_momentum+=gas_mass*a;
    for(int k=0;k<100;++k){const auto drag=flow::stokes_drag(c,q,flow::gather_velocity(exchange_air,stencil));flow::MomentumSource r;r.u_force_n.assign(l.u_count(),0);r.v_force_n.assign(l.v_count(),0);flow::scatter_reaction(r,stencil,drag.force_n);q.velocity_m_s.x+=dt*drag.force_n.x/mass;for(std::size_t i=0;i<r.u_force_n.size();++i)exchange_air.u_m_s[i]+=dt*r.u_force_n[i]/gas_mass;}
    const double slip=flow::gather_velocity(exchange_air,stencil).x-q.velocity_m_s.x,expected=.02*std::exp(-1.);
    check(std::abs(slip/expected-1)<.006,"exchange analytic decay");double momentum=mass*q.velocity_m_s.x;for(double a:exchange_air.u_m_s)momentum+=gas_mass*a;check(std::abs(momentum-initial_momentum)<1e-20,"exchange momentum conservation");out<<"exchange_decay_relative_error,"<<std::abs(slip/expected-1)<<','<<std::abs(momentum-initial_momentum)<<'\n';
    flow::TwoWayState state{flow::make_initial_fields(c),{p}},repeat=state;auto empty=state;empty.particles.clear();auto off=state;auto off_settings=s;off_settings.reaction_enabled=false;double maxexchange=0;
    for(std::uint64_t k=0;k<d.steps;++k){auto report=flow::advance_two_way(c,s,state);maxexchange=std::max(maxexchange,report.exchange_residual_n_s);flow::advance_two_way(c,s,repeat);flow::advance_two_way(c,s,empty);flow::advance_two_way(c,off_settings,off);}
    check(state.air.u_m_s==repeat.air.u_m_s&&state.particles[0].velocity_m_s.x==repeat.particles[0].velocity_m_s.x,"repeat differs");check(empty.air.u_m_s==off.air.u_m_s,"matched one-way air changed");
    double difference=0;for(std::size_t i=0;i<state.air.u_m_s.size();++i)difference=std::max(difference,std::abs(state.air.u_m_s[i]-off.air.u_m_s[i]));
    check(difference>1e-8&&state.particles[0].velocity_m_s.x<off.particles[0].velocity_m_s.x,"reaction did not affect air and particle");out<<"actual_exchange_residual_Ns,"<<maxexchange<<",0\nreaction_air_difference_m_s,"<<difference<<",0\n";
    double tv[3];int n=0;for(int sub:{5,10,20})tv[n++]=run(16,sub,true);double ratio=std::abs((tv[0]-tv[1])/(tv[1]-tv[2]));check(ratio>1.7&&ratio<2.3,"coupling time refinement");out<<"coupling_time_ratio,"<<ratio<<",0\n";
    double grid[3];n=0;for(int ny:{8,16,32}){grid[n++]=run(ny,20,true);out<<"grid_velocity_"<<ny<<','<<grid[n-1]<<",0\n";}
    check(std::abs(grid[2]-grid[1])<std::abs(grid[1]-grid[0]),"fixed-kernel grid difference not decreasing");check(std::abs((grid[2]-grid[1])/grid[2])<.02,"fine grid sensitivity above 2 percent");
    out<<"grid_difference_ratio,"<<std::abs((grid[0]-grid[1])/(grid[1]-grid[2]))<<",0\n";
    // Off-node particle: quadrature alignment need not be monotone, but the
    // refined velocities must remain close at fixed physical support.
    double shifted[3];n=0;
    for(int ny:{8,16,32}){auto cc=channel(ny);auto pp=p;pp.position_m={.01017,.00513,0};flow::TwoWayState test{flow::make_initial_fields(cc),{pp}};for(std::uint64_t k=0;k<flow::validate(cc).steps;++k)flow::advance_two_way(cc,s,test);shifted[n++]=test.particles[0].velocity_m_s.x;}
    check(std::abs((shifted[2]-shifted[1])/shifted[2])<.02,"off-node grid sensitivity above 2 percent");out<<"off_node_fine_grid_relative_difference,"<<std::abs((shifted[2]-shifted[1])/shifted[2])<<",0\n";
    auto a=p,b=p;a.id=2;b.id=1;a.position_m.x=.009;b.position_m.x=.011;
    flow::TwoWayState pair{flow::make_initial_fields(c),{a,b}},reordered{pair.air,{b,a}};
    auto pair_report=flow::advance_two_way(c,s,pair);flow::advance_two_way(c,s,reordered);
    check(pair.air.u_m_s==reordered.air.u_m_s&&pair.particles[0].id==1&&pair.particles[0].velocity_m_s.x==reordered.particles[0].velocity_m_s.x,"particle-order dependence");
    check(std::abs(pair_report.volume.particle_volume_m3/(2*volume.particle_volume_m3)-1)<1e-14,"two-particle volume");out<<"overlapping_kernel_exchange_residual_Ns,"<<pair_report.exchange_residual_n_s<<",0\n";
    // Weak coupling: larger physical slab mass approaches matched one-way.
    auto weakc=c;weakc.grid.reporting_depth_m=1;flow::TwoWayState weak{flow::make_initial_fields(weakc),{p}},weakoff=weak;
    for(std::uint64_t k=0;k<d.steps;++k){flow::advance_two_way(weakc,s,weak);flow::advance_two_way(weakc,off_settings,weakoff);}
    const double weakerror=std::abs(weak.particles[0].velocity_m_s.x-weakoff.particles[0].velocity_m_s.x),strongerror=std::abs(state.particles[0].velocity_m_s.x-off.particles[0].velocity_m_s.x);
    check(weakerror<strongerror*.01,"weak limit did not approach one-way");out<<"weak_to_strong_difference_ratio,"<<weakerror/strongerror<<",0\n";
    auto badsettings=s;badsettings.coupling_substeps=1;flow::TwoWayState bad{flow::make_initial_fields(c),{p}};rejects([&]{flow::advance_two_way(c,badsettings,bad);});
    auto broken=c;broken.projection.max_iterations=1;auto saved=bad;rejects([&]{flow::advance_two_way(broken,s,bad);});check(bad.air.accepted_steps==0&&bad.air.u_m_s==saved.air.u_m_s&&bad.particles[0].position_m.x==saved.particles[0].position_m.x,"failed step committed");
    auto edge=p;edge.position_m.x=s.kernel_radius_m+std::max(d.dx_m,d.dy_m)+1e-8;edge.velocity_m_s.x=-.01;bad={flow::make_initial_fields(c),{edge}};saved=bad;
    rejects([&]{flow::advance_two_way(c,s,bad);});check(bad.air.accepted_steps==0&&bad.air.u_m_s==saved.air.u_m_s&&bad.particles[0].position_m.x==edge.position_m.x,"post-air boundary failure did not roll back");
    rejects([&]{flow::particle_stencil(c,{.001,.005,0},s.kernel_radius_m);});auto large=p;large.diameter_m=.04;bad.particles={large};rejects([&]{flow::validate_two_way(c,s,bad);});
    // Force-source validation and exact zero-source backward compatibility.
    auto pure=flow::make_initial_fields(c),forced=pure;flow::MomentumSource zero;zero.u_force_n.assign(l.u_count(),0);zero.v_force_n.assign(l.v_count(),0);flow::advance(c,pure);flow::advance_with_source(c,forced,zero);check(pure.u_m_s==forced.u_m_s&&pure.pressure_gauge_pa==forced.pressure_gauge_pa,"zero source changed air baseline");
    zero.u_force_n[0]=1;rejects([&]{flow::advance_with_source(c,forced,zero);});out<<"rejection_and_rollback,1,0\n";
    std::cout<<"PASS dilute two-way coupling\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
