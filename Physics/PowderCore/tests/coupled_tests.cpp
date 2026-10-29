#include "powder/coupled.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <iomanip>
using namespace powder;
void ok(bool b,const char* s){if(!b)throw std::runtime_error(s);}
template<class F>void reject(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}ok(caught,"expected rejection");}
double speed(const GasGrid& g){double s=0;for(std::size_t i=0;i<g.cells.size();++i)if(!g.cells[i].solid)s+=std::abs(g.velocity(int(i)).x);return s/g.cells.size();}
int main(){try{
 // Existing rigid-body force path: no hidden forces; consume only on commit.
 auto w=coupled_demo(1,0);auto& b=w.dem.bodies[0];w.dem.gravity_m_s2={};double mass=clump_properties(b.pebbles,b.density_kg_m3).mass_kg;
 w.dem.external_wrenches[b.id].force_n={mass,0,0};w.dem.step(1e-6);ok(std::abs(w.dem.bodies[0].velocity_m_s.x-1e-6)<1e-12&&w.dem.external_wrenches.empty(),"external wrench integration");
 GasConfig c;c.cells={6,4,4};c.boundary_pa={};GasGrid g(c);double initial=g.mass();
 for(std::size_t i=0;i<g.cells.size();++i)g.cells[i].epsilon=i%2?.6:1;g.initialize_pressure();initial=g.mass();
 for(int i=0;i<100;++i)g.step(1e-6);ok(speed(g)<1e-12&&std::abs(g.mass()-initial)<1e-15,"uniform-pressure variable-porosity equilibrium");
 auto before=g.cells;reject([&]{g.step(.1);});ok(g.cells[0].mass==before[0].mass,"gas CFL rollback");
 c.boundary_pa={101325,101300,0,0,0,0};GasGrid driven(c);initial=driven.mass();for(int i=0;i<300;++i)driven.step(1e-6);
 ok(driven.velocity(driven.index(3,2,2)).x>0,"pressure drives positive flow");ok(std::abs(driven.mass()+driven.boundary_mass_out-initial)<1e-14,"gas open boundary conservation");
 std::swap(c.boundary_pa[0],c.boundary_pa[1]);GasGrid reversed(c);for(int i=0;i<300;++i)reversed.step(1e-6);ok(reversed.velocity(reversed.index(3,2,2)).x<0,"reversed pressure flow");
 // Reverse an already moving gas field, and account for an additional pressure port.
 driven.config.boundary_pa={101300,101325,0,0,0,0};
 for(int i=0;i<2000;++i)driven.step(1e-6);
 ok(driven.velocity(driven.index(3,2,2)).x<0,"transient pressure reversal");
 GasConfig ventConfig;ventConfig.boundary_pa={101325,101300,0,101310,0,0};GasGrid vent(ventConfig);double ventInitial=vent.mass();
 for(int i=0;i<1000;++i)vent.step(1e-6);
 ok(std::abs(vent.mass()+vent.boundary_mass_out-ventInitial)<1e-13,"additional pressure port mass conservation");
 ok(dense_drag_beta(.003,900,1.2,1.8e-5,.5,1)>dense_drag_beta(.003,900,1.2,1.8e-5,1,1),"dense drag strengthens");
 auto coupled=coupled_demo(8,25);coupled.dem.gravity_m_s2={};for(auto& q:coupled.dem.bodies)q.velocity_m_s.x=.2;coupled.initialize();
 double volume=0;for(auto& q:coupled.dem.bodies)for(auto& p:clump_particles(q))volume+=3.141592653589793/6*std::pow(p.diameter_m,3);double gridVolume=0;for(auto& q:coupled.gas.cells)gridVolume+=(1-q.epsilon)*std::pow(coupled.gas.config.h,3);ok(std::abs(volume-gridVolume)<1e-16,"volume mapping conservation");
 for(int i=0;i<200;++i){auto r=coupled.step(1e-6);ok(r.exchange_error<1e-17&&std::abs(r.gas_balance_error)<1e-14&&std::abs(r.particle_balance_error)<1e-15,"two-way conservation");}
 auto count=coupled.dem.bodies.size();double time=coupled.dem.physical_time_s;reject([&]{coupled.step(.1);});ok(coupled.dem.physical_time_s==time&&coupled.dem.bodies.size()==count,"coupled rollback");
 auto extra=make_clump(999,0,{{{},.003}},900);extra.position_m={.15,.04,.04};coupled.inject({extra});ok(coupled.dem.bodies.size()==count+1&&std::abs(coupled.report().particle_balance_error)<1e-15,"injection ledger");reject([&]{coupled.inject({extra});});
 auto out=coupled_demo(1,0);out.dem.gravity_m_s2={};out.dem.bodies[0].position_m={out.gas.extent().x-1e-6,.04,.04};out.dem.bodies[0].velocity_m_s={2,0,0};out.initialize();out.step(1e-6);ok(out.dem.bodies.empty()&&out.exited_mass>0&&std::abs(out.report().particle_balance_error)<1e-15,"outlet body ledger");out.step(1e-6);
 auto blocked=coupled_demo(1,25);blocked.dem.bodies.clear();blocked.dem.gravity_m_s2={};add_coupled_obstacle(blocked,{8,0,0},{9,6,6});blocked.initialize();double left=0;for(int i=0;i<300;++i)blocked.step(1e-6);for(int z=0;z<6;++z)for(int y=0;y<6;++y)left+=std::abs(blocked.gas.velocity(blocked.gas.index(7,y,z)).x);ok(left<1e-8,"solid slab prevents cross flow");
 // Linear standing acoustic wave: independent analytic velocity and grid refinement.
 for(bool adaptive:{false,true}){double acousticError[3];for(int level=0;level<3;++level){GasConfig ac;ac.cells={8<<level,2,2};ac.h=.32/ac.cells[0];ac.boundary_pa={};ac.slip_walls=true;GasGrid wave(ac);
  const double amplitude=1e-5,cs=std::sqrt(287.05*ac.temperature_k),end=.0002;
  for(std::size_t i=0;i<wave.cells.size();++i)wave.cells[i].mass*=1+amplitude*std::cos(3.141592653589793*wave.centre(int(i)).x/.32);
  int steps=int(std::ceil(end/(.1*ac.h/(3*cs))));double dt=end/steps;
  if(adaptive){double remaining=end;while(remaining>1e-14){double step=std::min(remaining,wave.explicit_dt_limit());wave.step(step);remaining-=step;}}else for(int i=0;i<steps;++i)wave.step(dt);
  double error=0;for(int x=0;x<ac.cells[0];++x){int i=wave.index(x,0,0);double exact=amplitude*cs*std::sin(3.141592653589793*wave.centre(i).x/.32)*std::sin(cs*3.141592653589793*end/.32);error+=std::pow(wave.velocity(i).x-exact,2);}
  acousticError[level]=std::sqrt(error/ac.cells[0]);
 }
 std::cout<<"adaptive="<<adaptive<<" acoustic RMS errors "<<acousticError[0]<<' '<<acousticError[1]<<' '<<acousticError[2]<<'\n';ok(acousticError[1]<acousticError[0]*.8&&acousticError[2]<acousticError[1]*.8,"acoustic grid convergence");}
 GasGrid limit;double allowed=limit.explicit_dt_limit();limit.step(allowed);reject([&]{limit.explicit_dt_limit(.41);});reject([&]{limit.step(allowed*2);});
 // Air can lift particles against gravity; the smaller/lighter species responds faster.
 auto lift=coupled_demo(2,0,0,293.15,false,2);
 for(auto& cell:lift.gas.cells)cell.momentum={0,0,20*cell.mass};
 for(int i=0;i<100;++i)lift.step(1e-6);
 ok(lift.dem.bodies[0].velocity_m_s.z>0&&lift.dem.bodies[0].velocity_m_s.z>lift.dem.bodies[1].velocity_m_s.z,"air uplift and size/density response");
 // Contact-only adhesion supports an outward load below pull-off in the coupled path.
 double held=0,free=0;for(int adhesive=0;adhesive<2;++adhesive){auto wall=coupled_demo(1,0);wall.gas.config.boundary_pa={};wall.dem.gravity_m_s2={0,0,1};double radius=.0015;double m=clump_properties(wall.dem.bodies[0].pebbles,900).mass_kg;double attraction=4*3.141592653589793*.001*radius;
  wall.dem.wall_models[0].adhesion_gamma_j_m2=adhesive?.001:0;wall.dem.bodies[0].position_m={.1,.05,radius-(attraction-m)/10};wall.initialize();
  for(int i=0;i<1000;++i)wall.step(5e-6);if(adhesive)held=wall.dem.bodies[0].position_m.z;else free=wall.dem.bodies[0].position_m.z;
 }
 ok(held<.0015&&free>.0015+1e-6,"coupled wall hold and release");
 // Same obstacle is impermeable to gas and mechanically blocks grains.
 auto hit=coupled_demo(1,0);hit.dem.gravity_m_s2={};add_coupled_obstacle(hit,{8,0,0},{9,6,6});hit.dem.bodies[0].position_m={.15849,.05,.05};hit.dem.bodies[0].velocity_m_s={.02,0,0};hit.initialize();
 for(int i=0;i<1000;++i)hit.step(5e-6);ok(hit.dem.bodies[0].position_m.x<.1586&&hit.dem.bodies[0].velocity_m_s.x<.02,"particle slab contact");
 // Actual densely occupied mapping, not an artificial epsilon-only fixture.
 auto bed=coupled_demo(1,0);bed.dem.bodies.clear();bed.dem.gravity_m_s2={};bed.gas.config.boundary_pa={};
 for(int z=0;z<8;++z)for(int y=0;y<8;++y)for(int x=0;x<8;++x){auto q=make_clump(1+x+8*y+64*z,0,{{{},.004}},900);q.position_m={.05+x*.0042,.04+y*.0042,.04+z*.0042};q.velocity_m_s={.02,0,0};bed.dem.bodies.push_back(q);}bed.initialize();ok(bed.report().min_epsilon<.8,"dense volume fraction fixture");
 for(int i=0;i<10;++i){auto r=bed.step(5e-6);ok(r.exchange_error<1e-15&&std::abs(r.gas_balance_error)<1e-13,"dense coupled conservation");}
 double gasx=0;for(std::size_t i=0;i<bed.gas.cells.size();++i)gasx+=bed.gas.cells[i].momentum.x;ok(gasx>0,"particles drive gas in dense coupling");
 double velocities[3];for(int level=0;level<3;++level){auto refinement=coupled_demo(2,25);refinement.dem.gravity_m_s2={};for(auto& q:refinement.dem.bodies)q.velocity_m_s={1,0,0};refinement.initialize();for(int i=0;i<(100<<level);++i)refinement.step(5e-6/(1<<level));velocities[level]=refinement.dem.bodies[0].velocity_m_s.x;}
 std::cout<<std::setprecision(17)<<"coupled timestep velocities "<<velocities[0]<<' '<<velocities[1]<<' '<<velocities[2]<<'\n';ok(std::abs(velocities[2]-velocities[1])<.8*std::abs(velocities[1]-velocities[0]),"coupled timestep convergence");
 // Non-spherical body in gas shear: opposite loads must generate rigid rotation.
 {
 auto shear=coupled_demo(3,0);auto body=shear.dem.bodies[2];body.position_m={.16,.06,.06};shear.dem.bodies={body};shear.dem.gravity_m_s2={};shear.gas.config.boundary_pa={};shear.initialize();
 for(std::size_t i=0;i<shear.gas.cells.size();++i){double uz=20*(shear.gas.centre(int(i)).x-.16);shear.gas.cells[i].momentum={0,0,uz*shear.gas.cells[i].mass};}
 shear.step(1e-6);
 ok(shear.dem.bodies[0].angular_momentum_world.y<0,"gas shear initial torque direction");
 for(int i=0;i<100;++i)shear.step(1e-6);
 const auto& spun=shear.dem.bodies[0];auto pebbles=clump_particles(spun);double spacing=std::hypot(pebbles[0].position_m.x-pebbles[1].position_m.x,pebbles[0].position_m.y-pebbles[1].position_m.y,pebbles[0].position_m.z-pebbles[1].position_m.z);
 ok(std::isfinite(spun.angular_momentum_world.y)&&std::abs(spun.angular_momentum_world.y)>0,"non-spherical rotation remains finite");ok(std::abs(spacing-.003)<1e-12,"gas-coupled non-spherical rigid spacing");ok(std::abs(shear.report().particle_balance_error)<1e-15&&shear.report().exchange_error<1e-17,"non-spherical exchange and mass ledger");
 }
 // Sequential pulses, including a real two-pebble body, leave through both open faces.
 {
 auto conveyor=coupled_demo(1,0);conveyor.dem.bodies.clear();conveyor.dem.gravity_m_s2={};conveyor.initialize();
 for(int cycle=0;cycle<4;++cycle){bool positive=cycle%2==0;auto incoming=make_clump(1000+cycle,2,{{{0,-.0015,0},.003},{{0,.0015,0},.003}},2400);incoming.position_m={positive?conveyor.gas.extent().x-1e-5:1e-5,.06,.06};incoming.velocity_m_s={positive?1.0:-1.0,0,0};conveyor.inject({incoming});
  for(int i=0;i<20;++i)conveyor.step(1e-6);
  ok(conveyor.dem.bodies.empty(),"sequential non-spherical pulse outlet");ok(std::abs(conveyor.report().particle_balance_error)<1e-15&&std::abs(conveyor.report().gas_balance_error)<1e-13,"sequential pulse conservation");reject([&]{conveyor.inject({incoming});});
 }
 ok(conveyor.exited_by_face[0]>0&&conveyor.exited_by_face[1]>0,"both directional outlet ledgers");ok(std::abs(conveyor.exited_by_type.at(2)-conveyor.injected_by_type.at(2))<1e-15,"species outlet conservation");
 }
 // Additional open face carries real particles as well as gas, with face accounting.
 {
 auto leak=coupled_demo(1,0);leak.dem.gravity_m_s2={};leak.gas.config.boundary_pa[3]=101300;
 leak.dem.planes.erase(std::remove_if(leak.dem.planes.begin(),leak.dem.planes.end(),[](const auto& p){return p.id==4;}),leak.dem.planes.end());
 leak.dem.bodies[0].position_m={.1,leak.gas.extent().y-1e-6,.04};leak.dem.bodies[0].velocity_m_s={0,2,0};leak.initialize();leak.step(1e-6);
 ok(leak.dem.bodies.empty()&&leak.exited_by_face[3]>0&&std::abs(leak.report().particle_balance_error)<1e-15,"additional port particle exit conservation");
 }
 {
 auto corner=coupled_demo(1,0,0,293.15,false,1);corner.dem.planes.clear();corner.dem.gravity_m_s2={};auto e=corner.gas.extent();corner.dem.bodies[0].position_m={e.x-1e-6,e.y-1e-6,.04};corner.dem.bodies[0].velocity_m_s={2,2,0};corner.initialize();reject([&]{corner.step(1e-6);});ok(corner.dem.physical_time_s==0&&corner.exited_mass==0,"open face cannot hide closed corner crossing");
 }
 {
 auto experiment=coupled_experiment(2);auto r=step_coupled_experiment(experiment,2,2e-7);ok(std::abs(r.time-2e-7)<1e-15&&experiment.minimum_step_s>0&&experiment.minimum_step_s<=2e-7,"experiment subdivision preserves requested time");reject([&]{step_coupled_experiment(experiment,2,1e-4);});reject([&]{coupled_experiment(7);});
 }
 // Independent ceiling force/lever-arm check for the actual foreign clump material.
 for(bool cached:{false,true})for(double dt:{1e-9,5e-10}){
 auto reference=coupled_experiment(2).dem;auto body=reference.bodies[2];reference.use_cached_sphere_path=cached;reference.planes={{1,{0,0,-1},-.01}};
 double mass=clump_properties(body.pebbles,body.density_kg_m3).mass_kg;auto material=reference.wall_models.at(body.type);double pull=4*3.141592653589793*material.adhesion_gamma_j_m2*.0005,kn=material.linear.kn;
 double angle=.6;body.orientation={std::cos(angle/2),0,-std::sin(angle/2),0};double delta=pull/(2*kn);body.position_m={.01,.01,.01-.0005-.0005*std::sin(angle)+delta};body.velocity_m_s={};body.angular_momentum_world={};reference.bodies={body};auto rep=reference.step(dt);auto next=reference.bodies[0];double upward=pull-kn*delta,expectedTorque=-.0005*std::cos(angle)*upward;
 double forceError=std::abs(mass*next.velocity_m_s.z/dt-(upward-mass*9.81));double torqueError=std::abs(next.angular_momentum_world.y/dt-expectedTorque);
 ok(rep.wall_contacts==1&&next.velocity_m_s.z<0,"single adhesive contact can accelerate tilted clump downward");ok(forceError<1e-12&&torqueError<1e-15,"ceiling force and lever arm match independent analytic wrench");
 auto balanced=coupled_experiment(2).dem;balanced.planes=reference.planes;body.orientation={1,0,0,0};body.position_m={.01,.01,.01-.0005+(2*pull-mass*9.81)/(2*kn)};balanced.bodies={body};auto flat=balanced.step(dt);
 ok(flat.wall_contacts==2&&std::abs(mass*balanced.bodies[0].velocity_m_s.z/dt)<1e-12&&std::abs(balanced.bodies[0].angular_momentum_world.y/dt)<1e-15,"symmetric double contact supports same clump without spurious torque");
 std::cout<<"clump ceiling analytic cached="<<cached<<" dt="<<dt<<" force_error_N="<<forceError<<" torque_error_Nm="<<torqueError<<'\n';
 }
 GasConfig hot;hot.temperature_k=350;GasGrid hg(hot);GasGrid cold;ok(hg.mass()<cold.mass()&&hot.viscosity()>cold.config.viscosity(),"temperature changes gas density/viscosity");
 std::cout<<"PASS external wrench, 3D porosity equilibrium, pressure/reversal, open mass, two-way impulse, volume, rollback, injection/outlet, slab, temperature\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
