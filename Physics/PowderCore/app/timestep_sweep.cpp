#include "powder/clump.hpp"
#include "powder/mixing.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
using namespace powder;
double norm(Vec3 v){return std::hypot(v.x,v.y,v.z);}
void check(bool b,const char* s){if(!b)throw std::invalid_argument(s);}
int main(int argc,char** argv){try{
 check(argc==4,"usage: powder_dt07 CASE DT NEW_DIRECTORY");std::string name=argv[1];std::size_t used=0;double dt=std::stod(argv[2],&used);check(used==std::string(argv[2]).size()&&std::isfinite(dt)&&dt>0,"invalid dt");
 bool collision=name=="collision_linear"||name=="collision_hertz"||name=="collision_hertz_gentle",mm=name=="mm_packing32",mix=name=="mixing5_short"||name=="mixing5_long"||name=="mixing30_diagnostic";
 check(collision||mm||mix||name=="packing32","unknown case");
 double radius=mm?.001:.02,density=mm?1500:2500,duration=mm?.0004:mix?(name=="mixing5_long"?.1:.02):.04;
 double ratio=duration/dt;check(ratio>=20&&ratio<=2000000&&std::abs(ratio-std::round(ratio))<1e-6,"nonintegral/oversized step count");auto steps=static_cast<std::uint64_t>(std::llround(ratio));check(steps%20==0,"20 common physical sample times required");
 auto out=std::filesystem::path(argv[3]);check(std::filesystem::create_directory(out),"output must be new");
 auto file=[&](const char* n){std::ofstream f(out/n);f<<std::setprecision(17);return f;};
 ClumpWorld w;MaterialContact m;m.law=(name=="collision_hertz"||name=="collision_hertz_gentle")?ContactLaw::HertzMindlin:ContactLaw::Linear;m.linear={10000,mm?.01:8,2500,mm?.0025:2,.4};m.cn=8;m.ct=2;m.mu=.4;m.a={1e7,.25};m.b=m.a;
 w.wall_models={{0,m},{1,m}};w.pair_models={{{0,0},m},{{0,1},m},{{1,1},m}};
 for(int i=0;i<(collision?2:32);++i){auto b=make_clump(i+1,collision?0:static_cast<unsigned>((i%4)>=2),{{{},2*radius}},density);
  if(collision){b.position_m={(i?1:-1)*(.0201),0,.1};b.velocity_m_s={(i?-1:1)*(name=="collision_hertz_gentle"?.02:.1),0,0};}
  else if(mix){int x=i%8,y=i/8;b.position_m={(x-3.5)*.06,(y<2?-1.:1.)*(y%2==0?.03001:.09001),.02005};b.type=x>=4?1:0;}
  else{double pitch=2*radius*.99999;b.position_m={(i%4-1.5)*pitch,((i/4)%4-1.5)*pitch,radius*.99999+(i/16)*pitch};}
  w.bodies.push_back(b);
 }
 if(collision)w.gravity_m_s2={};else{FixedBox floor;floor.id=1;floor.centre_m={0,0,-.05};floor.half_m={.5,.5,.05};w.meshes.emplace_back(triangulated_box(floor));}
 if(mix){FixedBox paddle;paddle.id=2;paddle.centre_m={0,0,.04};paddle.half_m={.24,.01,.03};PrescribedMotion motion;motion.pivot_m=paddle.centre_m;motion.rpm=name=="mixing30_diagnostic"?30:5;w.moving_meshes.emplace_back(StaticTriangleMesh(triangulated_box(paddle)),motion);}
 MixingConfig mc;mc.dt=dt;mc.duration_s=duration;mc.sample_s=duration/20;mc.density_a=mc.density_b=density;std::map<ParticleId,unsigned> groups;for(auto& b:w.bodies)groups[b.id]=b.type;
 auto state=file("states.csv");state<<"sample,time_s,id,x,y,z,vx,vy,vz,wx,wy,wz\n";
 auto series=file("series.csv");series<<"sample,time_s,com_z,energy_j,wall_work_j,torque_impulse_z,contact_integral_s,mixing_valid,mixing_index,outside\n";
 auto geometry=file("initial.csv");geometry<<serialize_clump_state(w);geometry.close();
 double totalmass=0,initialenergy=0;for(auto& b:w.bodies){totalmass+=clump_properties(b.pebbles,b.density_kg_m3).mass_kg;initialenergy+=clump_kinetic_energy(b);}
 double peakenergy=initialenergy,maxoverlap=0,contacts=0,impulse=0,peakforce=0,maxmomentum=0,minlimit=std::numeric_limits<double>::infinity(),minwall=minlimit,maxelastic=0,maxdamping=0,mobility=0,minmass=0,maxk=0,maxc=0;
 std::size_t maxdegree=0;std::uint64_t accepted=0;bool mixvalid=true;int outside=0;std::string error;
 auto sample=[&](int index){double com=0,energy=0;for(const auto& b:w.bodies){const auto omega=clump_omega(b);state<<index<<','<<index*duration/20<<','<<b.id<<','<<b.position_m.x<<','<<b.position_m.y<<','<<b.position_m.z<<','<<b.velocity_m_s.x<<','<<b.velocity_m_s.y<<','<<b.velocity_m_s.z<<','<<omega.x<<','<<omega.y<<','<<omega.z<<'\n';com+=b.position_m.z*clump_properties(b.pebbles,b.density_kg_m3).mass_kg;energy+=clump_kinetic_energy(b);}
  MixingSample ms;if(mix){ms=measure_mixing(mc,w.bodies,groups,accepted);mixvalid=mixvalid&&ms.valid;outside=std::max(outside,ms.outside);}
  series<<index<<','<<index*duration/20<<','<<com/totalmass<<','<<energy<<','<<w.moving_wall_work_j<<','<<impulse<<','<<contacts<<','<<(mix?ms.valid:true)<<','<<(mix?ms.index:0)<<','<<ms.outside<<'\n';
 };
 sample(0);const auto start=std::chrono::steady_clock::now();
 try{for(std::uint64_t i=1;i<=steps;++i){double oldvx=w.bodies[0].velocity_m_s.x;auto r=w.step(dt);accepted=i;
  maxoverlap=std::max(maxoverlap,r.max_overlap_m/radius);contacts+=(r.pair_contacts+r.wall_contacts)*dt;if(!r.moving_wall_loads.empty())impulse+=r.moving_wall_loads[0].reaction_torque_nm.z*dt;
  const auto& s=r.step_scale;minlimit=std::min(minlimit,s.contact_limit_s);minwall=std::min(minwall,s.moving_wall_limit_s);maxdegree=std::max(maxdegree,s.maximum_contacts);maxelastic=std::max(maxelastic,s.elastic_rate_s);maxdamping=std::max(maxdamping,s.damping_rate_s);mobility=s.maximum_mobility;minmass=s.minimum_body_mass_kg;maxk=std::max(maxk,s.maximum_stiffness_n_m);maxc=std::max(maxc,s.maximum_damping_ns_m);
  if(collision){double energy=0;Vec3 momentum{};for(auto& b:w.bodies){energy+=clump_kinetic_energy(b);double mass=clump_properties(b.pebbles,b.density_kg_m3).mass_kg;momentum.x+=mass*b.velocity_m_s.x;momentum.y+=mass*b.velocity_m_s.y;momentum.z+=mass*b.velocity_m_s.z;}peakenergy=std::max(peakenergy,energy);maxmomentum=std::max(maxmomentum,norm(momentum));peakforce=std::max(peakforce,std::abs(w.bodies[0].velocity_m_s.x-oldvx)*totalmass/2/dt);}
  if(i%(steps/20)==0)sample(static_cast<int>(i/(steps/20)));
 }}catch(const std::exception& e){error=e.what();}
 double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 auto final=file("final.csv");final<<serialize_clump_state(w);
 auto summary=file("summary.csv");summary<<"case,dt_s,duration_s,accepted_steps,requested_steps,radius_m,density_kg_m3,mass_kg,max_overlap_ratio,contact_integral_s,torque_impulse_z,wall_work_j,peak_force_n,initial_energy_j,peak_energy_j,max_momentum_residual,min_contact_limit_s,min_wall_limit_s,max_degree,max_elastic_rate,max_damping_rate,max_mobility,min_body_mass,max_stiffness,max_damping,mixing_valid,outside,wall_s,rate,error\n";
 summary<<name<<','<<dt<<','<<duration<<','<<accepted<<','<<steps<<','<<radius<<','<<density<<','<<totalmass<<','<<maxoverlap<<','<<contacts<<','<<impulse<<','<<w.moving_wall_work_j<<','<<peakforce<<','<<initialenergy<<','<<peakenergy<<','<<maxmomentum<<','<<(std::isfinite(minlimit)?minlimit:0)<<','<<(std::isfinite(minwall)?minwall:0)<<','<<maxdegree<<','<<maxelastic<<','<<maxdamping<<','<<mobility<<','<<minmass<<','<<maxk<<','<<maxc<<','<<mixvalid<<','<<outside<<','<<wall<<','<<accepted*dt/wall<<','<<error<<'\n';
 std::cout<<name<<" dt="<<dt<<" accepted="<<accepted<<'/'<<steps<<" limit="<<minlimit<<" overlap="<<maxoverlap<<" error="<<error<<'\n';
 check(bool(state)&&bool(series)&&bool(summary)&&bool(final),"write failure");return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
