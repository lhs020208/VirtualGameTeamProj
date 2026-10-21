#include "powder/core.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

int checks=0;
void require(bool ok,const char* name) { ++checks; if(!ok) throw std::runtime_error(name); }
void near(double a,double b,const char* name) { require(std::abs(a-b)<=1e-12*std::max(std::abs(b),1e-300),name); }
void rejects(const std::function<void()>& fn,const char* name) {
    bool rejected=false;
    try { fn(); } catch(const std::invalid_argument&) { rejected=true; }
    require(rejected,name);
}
int main() {
    try {
        using namespace powder;
        near(metres_to_centimetres(1.25),125,"m to cm");
        near(centimetres_to_metres(-250),-2.5,"cm to m");
        require(centimetres_to_metres(0)==0,"zero position");
        const auto v=centimetres_to_metres(metres_to_centimetres(Vec3{1.25,-2.5,0}));
        near(v.x,1.25,"vector x"); near(v.y,-2.5,"vector y"); require(v.z==0,"vector z");
        // Independently tabulated homogeneous sphere: radius 1m, density 1kg/m3.
        const auto s=sphere_properties(2,{1});
        near(s.mass_kg,4.1887902047863909846,"unit sphere mass");
        near(s.inertia_kg_m2,1.6755160819145563938,"unit sphere inertia");
        const auto large=sphere_properties(4,{1});
        near(large.mass_kg/s.mass_kg,8,"diameter mass scaling");
        near(large.inertia_kg_m2/s.inertia_kg_m2,32,"diameter inertia scaling");
        const auto dense=sphere_properties(2,{7}); near(dense.mass_kg/s.mass_kg,7,"density scaling");
        for(double bad:{0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
            rejects([&]{sphere_properties(bad,{1000});},"invalid diameter");
            rejects([&]{sphere_properties(.001,{bad});},"invalid density");
            rejects([&]{FixedClock clock({bad,10,100});},"invalid dt");
        }
        rejects([]{sphere_properties(1e200,{1000});},"mass overflow");
        rejects([]{sphere_properties(1e-200,{1000});},"mass underflow");
        rejects([]{metres_to_centimetres(std::numeric_limits<double>::max());},"conversion overflow");
        rejects([]{centimetres_to_metres(std::numeric_limits<double>::denorm_min());},"conversion underflow");
        rejects([]{FixedClock c({1,9007199254740993ULL,0});},"tick range");
        rejects([]{FixedClock c({1e308,10,0});},"time overflow");
        Particle p; validate(p);
        rejects([&]{validate_particles({p,p});},"duplicate ids");
        p.orientation.w=2; rejects([&]{validate(p);},"quaternion norm");
        p.orientation.w=1; p.position_m.x=std::numeric_limits<double>::quiet_NaN();
        rejects([&]{validate(p);},"nonfinite position");
        FixedClock clock({.0001,10000,100});
        while(clock.advance()) {}
        require(clock.tick()==10000,"exact ticks"); near(clock.time_s(),1,"one second");
        require(!clock.advance(),"stop at requested ticks");
        clock.reset(); require(clock.tick()==0&&clock.time_s()==0,"reset");
        FixedClock empty({.001,0,0}); require(!empty.advance()&&empty.time_s()==0,"zero steps");
        FixedClock a({.001,1000,1}), b({.001,1000,1});
        while(a.advance()) {}
        for(int frame=0;frame<100;++frame) for(int j=0;j<10;++j) b.advance();
        require(a.time_s()==b.time_s(),"batching independent clock");
        Particle moving; moving.velocity_m_s={1,2,3};
        ForceAccumulator loads;
        const double mass=sphere_properties(moving.diameter_m,moving.material).mass_kg;
        loads.add({mass,0,0}); loads.add({2*mass,0,0});
        integrate_translation(moving,loads,.1);
        near(moving.velocity_m_s.x,1.3,"force superposition");
        near(moving.position_m.x,.13,"symplectic position");
        require(loads.total_n().x==0,"force cleared after integration");
        integrate_translation(moving,loads,.1);
        near(moving.velocity_m_s.x,1.3,"no stale force");
        loads.add({mass,0,0});
        rejects([&]{integrate_translation(moving,loads,0);},"invalid integration dt");
        near(moving.position_m.x,.26,"failed step preserves state");
        near(loads.total_n().x,mass,"failed step preserves force");
        rejects([&]{loads.add({std::numeric_limits<double>::infinity(),0,0});},"invalid force");
        moving.angular_velocity_rad_s.x=1;
        rejects([&]{integrate_translation(moving,loads,.1);},"unsupported rotation");
        moving.angular_velocity_rad_s.x=0;
        moving.velocity_m_s.x=std::numeric_limits<double>::max();
        rejects([&]{integrate_translation(moving,loads,2);},"integration overflow");
        std::cout<<"PASS "<<checks<<" checks; relative tolerance 1e-12\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
