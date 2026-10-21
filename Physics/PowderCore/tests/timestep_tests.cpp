#include "powder/clump.hpp"
#include <iostream>
using namespace powder;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"expected rejection");}
int main(){try{
 auto a=rigid_step_scale(10,10000,8,1);check(a.contact_limit_s==.02/(std::sqrt(2.*10000*10)+2.*8*10),"original formula preserved");
 check(rigid_step_scale(10,10000,8,8).contact_limit_s<a.contact_limit_s,"multi-contact");check(rigid_step_scale(80,10000,8,1).contact_limit_s<a.contact_limit_s,"smaller mass/inertia");check(rigid_step_scale(10,40000,8,1).contact_limit_s<a.contact_limit_s,"stiffness");check(rigid_step_scale(10,10000,80,1).contact_limit_s<a.contact_limit_s,"damping");check(rigid_step_scale(10,10000,0,1).damping_rate_s==0,"undamped");
 rejects([]{rigid_step_scale(0,1,0,1);});rejects([]{rigid_step_scale(1,1,-1,1);});
 FixedBox box;box.id=1;box.half_m={.1,.1,.1};PrescribedMotion m;m.rpm=5;MovingTriangleMesh wall(StaticTriangleMesh(triangulated_box(box)),m);
 double limit=wall.step_limit(.001);wall.validate_step(0,limit*.999,.001);rejects([&]{wall.validate_step(0,limit*1.001,.001);});check(wall.step_limit(.0005)<limit,"minimum radius wall travel");m.rpm=10;MovingTriangleMesh faster(StaticTriangleMesh(triangulated_box(box)),m);check(faster.step_limit(.001)<limit,"RPM");m.rpm=0;m.linear_velocity_m_s={10,0,0};MovingTriangleMesh translation(StaticTriangleMesh(triangulated_box(box)),m);check(std::abs(translation.step_limit(.001)-.000005)<1e-15,"translation limit");
 auto w=clump_demo();auto r=w.step(2e-6);check(r.step_scale.minimum_radius_m==.02&&r.step_scale.minimum_body_mass_kg>0&&r.step_scale.contact_limit_s>=2e-6,"actual step report");auto before=serialize_clump_state(w);rejects([&]{w.step(r.step_scale.contact_limit_s*1.01);});check(serialize_clump_state(w)==before,"reject leaves state unchanged");
 std::cout<<"PASS unchanged bound, mass/stiffness/damping/contact sensitivity, radius/RPM/translation guards and rejected state atomicity\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
