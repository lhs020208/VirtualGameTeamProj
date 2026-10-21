#include "powder/clump.hpp"
#include "powder/mixing.hpp"
#include "powder/softening.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
using namespace powder;
void check(bool b,const char* s){if(!b)throw std::invalid_argument(s);}
double parse(const char* s){std::size_t used=0;double v=std::stod(s,&used);check(used==std::string(s).size()&&std::isfinite(v),"invalid numeric input");return v;}
int main(int argc,char** argv){try{
 check(argc==5,"usage: powder_soft08 CASE STIFFNESS_RATIO DT NEW_DIRECTORY");
 const std::string name=argv[1];const double alpha=parse(argv[2]),dt=parse(argv[3]);
 const bool flow=name=="discharge_wide",mix=name=="mixing5_long",mm=name=="mm_packing32";
 check(flow||mix||mm||name=="packing32","unknown bulk fixture");
 const int count=flow?16:32;const double radius=mm?.001:.02,density=mm?1500:2500,duration=flow?.16:mix?.1:mm?.0004:.04;
 check(dt>0,"positive dt required");const double n=duration/dt;
 check(n>=20&&n<=2000000&&std::abs(n-std::round(n))<1e-6,"nonintegral/oversized run");const auto steps=static_cast<std::uint64_t>(std::llround(n));check(steps%20==0,"common physical sample grid required");
 MaterialContact physical;physical.law=ContactLaw::Linear;physical.linear={10000,mm?.01:8,2500,mm?.0025:2,.4};
 const auto model=soften_linear_bulk(physical,alpha,SofteningScope::BulkWideFlow);
 ClumpWorld w;w.pair_models={{{0,0},model.computational},{{0,1},model.computational},{{1,1},model.computational}};w.wall_models={{0,model.computational},{1,model.computational}};
 for(int i=0;i<count;++i){auto b=make_clump(i+1,static_cast<unsigned>((i%4)>=2),{{{},2*radius}},density);
  if(mix){int x=i%8,y=i/8;b.position_m={(x-3.5)*.06,(y<2?-1.:1.)*(y%2==0?.03001:.09001),.02005};b.type=x>=4?1:0;}
  else{double pitch=2*radius*.99999;b.position_m={(i%4-1.5)*pitch,((i/4)%4-1.5)*pitch,radius*.99999+(i/16)*pitch};}
  if(flow){b.position_m.x=(i%4-1.5)*(.2/3);b.position_m.z=.08;b.velocity_m_s={b.position_m.x>0?.03:-.03,0,-.2};}
  w.bodies.push_back(b);
 }
 if(flow){for(int side:{-1,1}){FixedBox wall;wall.id=side<0?1:2;wall.centre_m={side*.129995,0,.05};wall.half_m={.01,.2,.15};w.boxes.push_back(wall);}}
 else{FixedBox floor;floor.id=1;floor.centre_m={0,0,-.05};floor.half_m={.5,.5,.05};w.meshes.emplace_back(triangulated_box(floor));}
 if(mix){FixedBox paddle;paddle.id=2;paddle.centre_m={0,0,.04};paddle.half_m={.24,.01,.03};PrescribedMotion motion;motion.pivot_m=paddle.centre_m;motion.rpm=5;w.moving_meshes.emplace_back(StaticTriangleMesh(triangulated_box(paddle)),motion);}
 const auto out=std::filesystem::path(argv[4]);check(std::filesystem::create_directory(out),"output must be new");
 auto file=[&](const char* name_){std::ofstream f(out/name_);f<<std::setprecision(17);check(bool(f),"output open");return f;};
 auto manifest=file("materials.csv");manifest<<"role,scope,stiffness_ratio,damping_scale,physical_kn,computed_kn,physical_kt,computed_kt,physical_cn,computed_cn,physical_ct,computed_ct,physical_mu,computed_mu,physical_young_a,computed_young_a,overlap_cap\n";
 for(const char* role:{"pair00","pair01","pair11","wall0","wall1"}){const auto& p=model.physical.linear;const auto& c=model.computational.linear;manifest<<role<<",bulk_wide_flow,"<<alpha<<','<<model.damping_ratio<<','<<p.kn<<','<<c.kn<<','<<p.kt<<','<<c.kt<<','<<p.cn<<','<<c.cn<<','<<p.ct<<','<<c.ct<<','<<p.mu<<','<<c.mu<<','<<model.physical.a.young_pa<<','<<model.computational.a.young_pa<<','<<physical.max_overlap_radius_ratio<<'\n';}
 auto initial=file("initial.csv");initial<<serialize_clump_state(w);initial.close();
 auto state=file("states.csv");state<<"sample,time_s,id,x,y,z,vx,vy,vz,wx,wy,wz\n";
 auto series=file("series.csv");series<<"sample,time_s,com_z,energy_j,wall_work_j,torque_impulse_z,contact_integral_s,mixing_valid,mixing_index,outside,arrived_count,arrived_mass_kg,mean_residence_s,p95_residence_s,active_mass_kg\n";
 auto events=file("events.csv");events<<"id,group,crossing_s,mass_kg\n";
 MixingConfig mc;mc.dt=dt;mc.duration_s=duration;mc.sample_s=duration/20;mc.density_a=mc.density_b=density;
 std::map<ParticleId,unsigned> groups;double totalmass=0;std::array<double,2> initial_mass{},exited_mass{};
 for(const auto& b:w.bodies){groups[b.id]=b.type;double mass=clump_properties(b.pebbles,b.density_kg_m3).mass_kg;totalmass+=mass;initial_mass[b.type]+=mass;}
 std::vector<double> residence;double maxoverlap=0,contacts=0,impulse=0,maxmasserror=0,minlimit=std::numeric_limits<double>::infinity();std::uint64_t accepted=0;bool valid=true;int outside=0;std::string error;
 auto mean=[&]{return residence.empty()?0:std::accumulate(residence.begin(),residence.end(),0.)/residence.size();};
 auto p95=[&]{if(residence.empty())return 0.;auto sorted=residence;std::sort(sorted.begin(),sorted.end());return sorted[static_cast<std::size_t>(std::ceil(.95*sorted.size()))-1];};
 auto sample=[&](int index){double com=0,energy=0;std::array<double,2> active{};
  for(const auto& b:w.bodies){const auto omega=clump_omega(b);state<<index<<','<<index*duration/20<<','<<b.id<<','<<b.position_m.x<<','<<b.position_m.y<<','<<b.position_m.z<<','<<b.velocity_m_s.x<<','<<b.velocity_m_s.y<<','<<b.velocity_m_s.z<<','<<omega.x<<','<<omega.y<<','<<omega.z<<'\n';double mass=clump_properties(b.pebbles,b.density_kg_m3).mass_kg;active[b.type]+=mass;com+=mass*b.position_m.z;energy+=clump_kinetic_energy(b);}
  for(int g=0;g<2;++g)maxmasserror=std::max(maxmasserror,std::abs(initial_mass[g]-active[g]-exited_mass[g]));
  MixingSample ms;if(mix){ms=measure_mixing(mc,w.bodies,groups,accepted);valid=valid&&ms.valid;outside=std::max(outside,ms.outside);}
  series<<index<<','<<index*duration/20<<','<<com/totalmass<<','<<energy<<','<<w.moving_wall_work_j<<','<<impulse<<','<<contacts<<','<<(mix?ms.valid:true)<<','<<(mix?ms.index:0)<<','<<ms.outside<<','<<residence.size()<<','<<exited_mass[0]+exited_mass[1]<<','<<mean()<<','<<p95()<<','<<active[0]+active[1]<<'\n';
 };
 sample(0);const auto start=std::chrono::steady_clock::now();
 try{for(std::uint64_t step=1;step<=steps;++step){std::vector<ClumpBody> before;if(flow)before=w.bodies;auto report=w.step(dt);accepted=step;
  maxoverlap=std::max(maxoverlap,report.max_overlap_m/radius);contacts+=(report.pair_contacts+report.wall_contacts)*dt;minlimit=std::min(minlimit,report.step_scale.contact_limit_s);
  if(!report.moving_wall_loads.empty())impulse+=report.moving_wall_loads[0].reaction_torque_nm.z*dt;
  if(flow){std::vector<ClumpBody> remaining;for(const auto& b:w.bodies){if(b.position_m.z<0){auto old=std::find_if(before.begin(),before.end(),[&](const auto& v){return v.id==b.id;});check(old!=before.end()&&old->position_m.z>=0,"invalid outlet crossing");double fraction=old->position_m.z/(old->position_m.z-b.position_m.z);double crossing=(step-1+fraction)*dt,mass=clump_properties(b.pebbles,b.density_kg_m3).mass_kg;residence.push_back(crossing);exited_mass[b.type]+=mass;events<<b.id<<','<<b.type<<','<<crossing<<','<<mass<<'\n';}else remaining.push_back(b);}w.bodies=std::move(remaining);}
  if(step%(steps/20)==0)sample(static_cast<int>(step/(steps/20)));
 }}catch(const std::exception& e){error=e.what();}
 const double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();auto final=file("final.csv");final<<serialize_clump_state(w);
 auto summary=file("summary.csv");summary<<"case,alpha,dt_s,duration_s,accepted_steps,requested_steps,radius_m,density_kg_m3,mass_kg,count,max_overlap_ratio,contact_integral_s,torque_impulse_z,wall_work_j,min_contact_limit_s,mixing_valid,outside,arrived_count,arrived_mass_kg,mean_residence_s,p95_residence_s,max_mass_error,wall_s,rate,error\n";
 summary<<name<<','<<alpha<<','<<dt<<','<<duration<<','<<accepted<<','<<steps<<','<<radius<<','<<density<<','<<totalmass<<','<<count<<','<<maxoverlap<<','<<contacts<<','<<impulse<<','<<w.moving_wall_work_j<<','<<(std::isfinite(minlimit)?minlimit:0)<<','<<valid<<','<<outside<<','<<residence.size()<<','<<exited_mass[0]+exited_mass[1]<<','<<mean()<<','<<p95()<<','<<maxmasserror<<','<<wall<<','<<accepted*dt/wall<<','<<error<<'\n';
 for(auto* f:{&manifest,&state,&series,&events,&final,&summary}){f->close();check(bool(*f),"output write failed");}
 std::cout<<name<<" alpha="<<alpha<<" dt="<<dt<<" steps="<<accepted<<'/'<<steps<<" overlap="<<maxoverlap<<" arrived="<<residence.size()<<" error="<<error<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
