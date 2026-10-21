#include "powder/session.hpp"
#include <iostream>
#include <stdexcept>
using namespace powder;
void expect(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){try{
 ParticleSystem w;w.model.law=ContactLaw::Linear;
 Particle p;p.id=1;p.diameter_m=1;p.material.density_kg_m3=6/3.14159265358979323846;p.position_m={0,0,.499};p.velocity_m_s={.1,0,0};w.particles={p};w.planes={{1,{0,0,1},0}};
 Session a(w,1e-5),b(w,1e-5);
 a.frame(100);expect(a.steps()==0,"starts paused");expect(a.single_step()&&a.steps()==1&&!a.running(),"one step pauses");a.reset();
 a.toggle();b.toggle();
 // Same physical step reached with 30 versus 120 render callbacks per simulated batch.
 for(int i=0;i<30;++i)a.frame(400);
 for(int i=0;i<120;++i)b.frame(100);
 const auto x=a.world().particles[0],y=b.world().particles[0];
 expect(a.steps()==12000&&a.steps()==b.steps()&&a.simulation_seconds()==b.simulation_seconds(),"physical clocks equal");
 expect(x.position_m.x==y.position_m.x&&x.position_m.y==y.position_m.y&&x.position_m.z==y.position_m.z&&x.velocity_m_s.x==y.velocity_m_s.x&&x.velocity_m_s.y==y.velocity_m_s.y&&x.velocity_m_s.z==y.velocity_m_s.z&&x.angular_velocity_rad_s.y==y.angular_velocity_rad_s.y&&x.orientation.w==y.orientation.w,"frame partition invariant");
 expect(a.world().active_wall_histories()==1,"contact history exists");
 a.pause();a.frame(100);expect(a.steps()==12000,"paused no advancement");a.reset();expect(a.steps()==0&&!a.running()&&a.world().active_wall_histories()==0&&a.world().particles[0].position_m.z==p.position_m.z,"reset clears state and history");
 Session fresh(w,1e-5);a.single_step();fresh.single_step();expect(a.world().particles[0].velocity_m_s.x==fresh.world().particles[0].velocity_m_s.x,"reset friction matches fresh");
 Session bad(w,1);bad.toggle();bad.frame(20);expect(!bad.running()&&!bad.error().empty()&&bad.steps()==0,"failure stops without skipped step");bad.toggle();expect(!bad.running(),"fault cannot resume without reset");
 std::cout<<"PASS session: frame partition, controls, reset history, fault stop\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
