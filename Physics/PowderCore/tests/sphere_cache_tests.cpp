#include "powder/clump.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace powder;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void near(double a,double b){check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=1e-12+1e-9*std::max(std::abs(a),std::abs(b)),"cached/reference numerical mismatch");}
void near(Vec3 a,Vec3 b){near(a.x,b.x);near(a.y,b.y);near(a.z,b.z);}
void compare(const ClumpWorld& a,const ClumpWorld& b){check(a.bodies.size()==b.bodies.size()&&a.histories()==b.histories(),"count/history mismatch");near(a.physical_time_s,b.physical_time_s);near(a.moving_wall_work_j,b.moving_wall_work_j);for(std::size_t i=0;i<a.bodies.size();++i){const auto& x=a.bodies[i];const auto& y=b.bodies[i];near(x.position_m,y.position_m);near(x.velocity_m_s,y.velocity_m_s);near(x.angular_momentum_world,y.angular_momentum_world);near(x.orientation.w,y.orientation.w);near(x.orientation.x,y.orientation.x);near(x.orientation.y,y.orientation.y);near(x.orientation.z,y.orientation.z);near(clump_kinetic_energy(x),clump_kinetic_energy(y));}}
ClumpWorld fixture(int mode){auto w=clump_demo();w.bodies.resize(1);w.boxes.clear();auto& b=w.bodies[0];b.position_m={0,0,.019999};b.velocity_m_s={.01,.02,-.001};b.orientation={std::cos(.3),std::sin(.3),0,0};b.angular_momentum_world={1e-5,2e-5,3e-5};MaterialContact m;m.law=mode==4?ContactLaw::HertzMindlin:ContactLaw::Linear;m.linear={10000,8,2500,2,.4};m.rolling={1000,1,.1};m.adhesion_gamma_j_m2=.001;w.pair_models={{{0,0},m}};w.wall_models={{0,m}};
 if(mode==0){b.position_m.z=.5;}
 if(mode==1||mode==4)w.planes.push_back({1,{0,0,1},0});
 if(mode==2){auto c=b;c.id=2;c.position_m.x=.039999;c.velocity_m_s.x=-.01;w.bodies.push_back(c);}
 if(mode==3||mode==5){FixedBox box;box.id=2;box.centre_m={0,0,-.05};box.half_m={.5,.5,.05};if(mode==3)w.boxes={box};else{PrescribedMotion motion;motion.linear_velocity_m_s={.01,0,.001};w.moving_meshes.emplace_back(StaticTriangleMesh(triangulated_box(box)),motion);}}
 if(mode==6){b=make_clump(1,0,{{{-.02,0,0},.04},{{.02,0,0},.04}},2500);b.position_m={0,0,.019999};b.velocity_m_s={.01,.02,0};b.angular_momentum_world={1e-5,2e-5,3e-5};w.planes.push_back({1,{0,0,1},0});}
 return w;
}
int main(){try{
 for(int mode=0;mode<7;++mode){auto a=fixture(mode),b=a;b.use_cached_sphere_path=false;ClumpProfile p;std::size_t contacts=0;for(int i=0;i<4000;++i){auto ar=a.step_profiled(1e-7,&p),br=b.step(1e-7);check(ar.pair_contacts==br.pair_contacts&&ar.wall_contacts==br.wall_contacts,"contact mismatch");contacts+=ar.pair_contacts+ar.wall_contacts;near(ar.max_overlap_m,br.max_overlap_m);near(ar.mass_kg,br.mass_kg);near(ar.moving_wall_work_j,br.moving_wall_work_j);for(std::size_t j=0;j<ar.moving_wall_loads.size();++j){near(ar.moving_wall_loads[j].reaction_force_n,br.moving_wall_loads[j].reaction_force_n);near(ar.moving_wall_loads[j].reaction_torque_nm,br.moving_wall_loads[j].reaction_torque_nm);}compare(a,b);}
 check(mode==0?contacts==0:contacts>0,"fixture did not exercise contact");check(p.property_rebuilds==a.bodies.size()&&p.material_table_rebuilds==1,"cache warmup");check(mode==6?p.generic_integrations==4000:p.sphere_integrations==4000*a.bodies.size(),"wrong integration path");a.reset_history();b.reset_history();a.step(1e-7);b.step(1e-7);compare(a,b);}
 auto w=fixture(0);ClumpProfile p;auto step=[&]{w.step_profiled(1e-7,&p);};step();step();check(p.property_rebuilds==1&&p.property_hits==1,"cache reuse");
 w.bodies[0].density_kg_m3=3000;step();w.bodies[0].pebbles[0].diameter_m=.03;step();check(p.property_rebuilds==3,"density/diameter invalidation");
 w.bodies[0].pebbles={{{-.02,0,0},.03},{{.02,0,0},.03}};step();check(p.property_rebuilds==4&&p.generic_integrations==1,"shape invalidation");
 auto reference=w;reference.use_cached_sphere_path=false;auto changed=w;changed.bodies[0].pebbles[0].local_m.x=-.03;changed.bodies[0].pebbles[1].local_m.x=.03;reference.bodies=changed.bodies;changed.step(1e-7);reference.step(1e-7);compare(changed,reference);
 w.wall_models[0].linear.mu=.2;step();w.pair_models[{0,0}].rolling.mu=.2;step();check(p.material_table_rebuilds==3,"material invalidation");
 w.wall_models[1]=w.wall_models[0];w.pair_models[{1,1}]=w.pair_models[{0,0}];w.bodies[0].type=1;step();check(p.material_table_rebuilds==4,"type invalidation");
 auto extra=w.bodies[0];extra.id=4;extra.position_m.x=1;w.bodies.push_back(extra);step();std::reverse(w.bodies.begin(),w.bodies.end());step();check(p.property_rebuilds==5,"add/reorder cache");w.bodies.pop_back();step();w.bodies.clear();step();w.bodies.push_back(extra);step();check(p.property_rebuilds==6,"remove/reinsert cache");
 w.invalidate_caches();step();check(p.property_rebuilds==7,"explicit invalidation");
 // Invalid edits and failed steps must not poison restored cache or mutate physics/history.
 auto rejects=[&](ClumpWorld bad){auto state=serialize_clump_state(w);const auto time=bad.physical_time_s;bool caught=false;try{bad.step(1e-7);}catch(const std::invalid_argument&){caught=true;}check(caught&&bad.physical_time_s==time,"invalid edit accepted or clock changed");check(state==serialize_clump_state(w),"copy mutated original cache/state");};
 auto bad=w;bad.wall_models[1].linear.kn=-1;rejects(bad);bad=w;bad.pair_models.clear();rejects(bad);bad=w;bad.bodies[0].density_kg_m3=-1;rejects(bad);bad=w;bad.bodies[0].pebbles[0].diameter_m=-1;rejects(bad);bad=w;bad.bodies[0].position_m.x=std::numeric_limits<double>::quiet_NaN();rejects(bad);bad=w;bad.bodies[0].orientation.w=2;rejects(bad);bad=w;bad.bodies.push_back(bad.bodies[0]);rejects(bad);
 const auto state=serialize_clump_state(w);bool caught=false;try{w.step(1);}catch(const std::invalid_argument&){caught=true;}check(caught&&state==serialize_clump_state(w),"failed dt atomicity");auto ref=w;ref.use_cached_sphere_path=false;w.step(1e-7);ref.step(1e-7);compare(w,ref);
 std::cout<<"PASS sphere/generic parity, fall/pair/plane/box/Hertz/rolling/adhesion/moving-wall, cache edits, copies and rollback\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
