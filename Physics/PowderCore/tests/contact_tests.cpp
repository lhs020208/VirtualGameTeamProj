#include "powder/core.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace powder;
constexpr double pi=3.14159265358979323846;
void check(bool v,const char* msg) { if(!v) throw std::runtime_error(msg); }
void near(double a,double b,double tol,const char* msg) { check(std::abs(a-b)<=tol,msg); }
struct Impact { double rebound,time,overlap,energy_error; };
Impact impact(double dt,double damping) {
    Particle p; p.diameter_m=1; p.material.density_kg_m3=6/pi;
    p.position_m.z=.5; p.velocity_m_s.z=-1;
    PlaneContact wall{0,10000,damping};
    double peak=0, max_energy_error=0;
    for(int i=0;i<200000;++i) {
        const auto before=p;
        ForceAccumulator f; step_with_plane(p,f,wall,dt);
        const auto contact=plane_contact(p,wall);
        peak=std::max(peak,contact.overlap_m);
        const double energy=.5*p.velocity_m_s.z*p.velocity_m_s.z+contact.elastic_energy_j;
        max_energy_error=std::max(max_energy_error,std::abs(energy-.5));
        if(p.position_m.z>=.5 && p.velocity_m_s.z>0) {
            const double fraction=(.5-before.position_m.z)/(p.position_m.z-before.position_m.z);
            return {p.velocity_m_s.z,(i+fraction)*dt,peak,max_energy_error};
        }
    }
    throw std::runtime_error("impact never exited");
}
int main() {
 try {
    // Acceptance fixed before execution: elastic e 0.002, tc 5e-5 s,
    // peak 2e-5 m; fine damped e 3e-4, tc 5e-6 s, peak 2e-6 m.
    const auto elastic=impact(1e-5,0);
    near(elastic.rebound,1,.002,"elastic restitution");
    near(elastic.time,pi/100,5e-5,"elastic contact time");
    near(elastic.overlap,.01,2e-5,"elastic peak");
    check(elastic.energy_error<.001,"elastic bounded energy error");
    const double zeta=.2, omega=100, wd=omega*std::sqrt(1-zeta*zeta);
    const double tpeak=std::atan(wd/(zeta*omega))/wd;
    const double exact_e=std::exp(-zeta*omega*tpeak);
    const double exact_t=tpeak+pi/(2*omega);
    const auto coarse=impact(1e-5,40), fine=impact(1e-6,40);
    near(fine.rebound,exact_e,3e-4,"damped restitution");
    near(fine.time,exact_t,5e-6,"damped contact time");
    near(fine.overlap,exact_e/omega,2e-6,"damped peak");
    check(std::abs(fine.rebound-exact_e)<std::abs(coarse.rebound-exact_e),"restitution convergence");
    check(fine.rebound<1,"damping loses energy");
    Particle p; p.diameter_m=1; p.material.density_kg_m3=6/pi;
    PlaneContact wall{0,10000,40};
    p.position_m.z=.5-9.81/10000;
    for(int i=0;i<10000;++i) { ForceAccumulator f; f.add_gravity(1,{0,0,-9.81}); step_with_plane(p,f,wall,1e-5); }
    near(p.position_m.z,.5-9.81/10000,1e-10,"static equilibrium");
    near(p.velocity_m_s.z,0,1e-10,"static speed");
    p.position_m.z=.49; p.velocity_m_s.z=100;
    check(plane_contact(p,wall).normal_force_n>0,"no attractive force on unloading");
    p.position_m.z=1; check(plane_contact(p,wall).normal_force_n==0,"no contact above plane");
    ForceAccumulator f; f.add({0,0,1}); bool rejected=false;
    try { step_with_plane(p,f,wall,1); } catch(const std::invalid_argument&) { rejected=true; }
    check(rejected && p.position_m.z==1 && f.total_n().z==1,"large dt rejection atomic");
    for(double bad:{-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        rejected=false; wall.damping_n_s_m=bad;
        try { plane_contact(p,wall); } catch(const std::invalid_argument&) { rejected=true; }
        check(rejected,"invalid damping");
    }
    std::cout.precision(12);
    std::cout<<"PASS plane contact\nelastic: e="<<elastic.rebound<<" tc="<<elastic.time<<" peak="<<elastic.overlap<<" maxEnergyError="<<elastic.energy_error
      <<"\ndamped reference: e="<<exact_e<<" tc="<<exact_t<<" peak="<<exact_e/omega
      <<"\ndamped fine: e="<<fine.rebound<<" tc="<<fine.time<<" peak="<<fine.overlap
      <<"\nrestitution errors coarse/fine: "<<std::abs(coarse.rebound-exact_e)<<" / "<<std::abs(fine.rebound-exact_e)<<'\n';
    return 0;
 } catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}
