#include "powder/clump.hpp"
#include "powder/mixing.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
using namespace powder;
namespace {
using Clock=std::chrono::steady_clock;
double elapsed(Clock::time_point t){return std::chrono::duration<double>(Clock::now()-t).count();}
void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct Case {std::string name,kind;int count=0;double radius=0,density=0,kn=0,cn=0,kt=0,ct=0,mu=0,dt=0;int steps=0,window=0;double rpm=0;std::string expected;};
std::vector<Case> read(const std::filesystem::path& path){check(std::filesystem::file_size(path)<65536,"fixture too large");std::ifstream f(path);std::string tag;int version=0;f>>tag>>version;check(tag=="POWDER_OPT01"&&version==1,"fixture version");std::vector<Case> cases;std::set<std::string> names;
while(f>>tag){if(tag=="end")break;check(tag=="case","case token");Case c;f>>c.name>>c.kind>>c.count>>c.radius>>c.density>>c.kn>>c.cn>>c.kt>>c.ct>>c.mu>>c.dt>>c.steps>>c.window>>c.rpm>>c.expected;check(bool(f),"truncated case");check(!c.name.empty()&&c.name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")==std::string::npos&&names.insert(c.name).second,"case name");check(c.kind=="freefall"||c.kind=="packing"||c.kind=="discharge"||c.kind=="mixing","case kind");check(c.count>=2&&c.count<=20000000&&c.count%2==0&&c.radius>0&&c.density>0&&c.kn>0&&c.cn>=0&&c.kt>0&&c.ct>=0&&c.mu>=0&&c.dt>0&&c.steps>0&&c.steps<=1000000&&c.window>0&&c.steps%c.window==0,"case bounds");check(c.expected=="pass"||c.expected=="reject_overlap","expected outcome");for(double v:{c.rpm,c.radius,c.density,c.kn,c.cn,c.kt,c.ct,c.mu,c.dt})check(std::isfinite(v),"nonfinite input");cases.push_back(c);check(cases.size()<=32,"too many cases");}
check(tag=="end"&&!cases.empty(),"missing end");check(!(f>>tag),"trailing input");return cases;}
void write(const std::filesystem::path& p,const std::string& s){std::ofstream f(p,std::ios::binary);f<<s;f.close();check(bool(f),"output write");}
std::ofstream output(const std::filesystem::path& p,const char* header){std::ofstream f(p);f.imbue(std::locale::classic());f<<std::setprecision(17)<<header<<'\n';check(bool(f),"output open");return f;}
ClumpWorld fixture(const Case& c){ClumpWorld w;MaterialContact m;m.law=ContactLaw::Linear;m.linear={c.kn,c.cn,c.kt,c.ct,c.mu};w.wall_models={{0,m},{1,m}};w.pair_models={{{0,0},m},{{0,1},m},{{1,1},m}};
const double d=2*c.radius;int columns=c.count<=32?4:8;double pitch=d*(c.kind=="packing"?.99999:1.5);
for(int i=0;i<c.count;++i){int col=i%columns,row=(i/columns)%columns,layer=i/(columns*columns);auto b=make_clump(static_cast<ParticleId>(i+1),static_cast<unsigned>(col>=columns/2),{{{},d}},c.density);b.position_m={(col-(columns-1)/2.)*pitch,(row-(columns-1)/2.)*pitch,c.radius*(c.kind=="packing"?.99999:3)+layer*pitch};
if(c.kind=="freefall")b.position_m.z+=.2;
if(c.kind=="discharge"){b.position_m.z=.01;b.position_m.x=(i%8-3.5)*d*1.5;b.position_m.y=(i/8-1.5)*d*1.5;b.velocity_m_s.z=-.5;}
if(c.kind=="mixing"){check(c.count==32&&c.radius==.02,"mixing recipe requires 32 legacy 20 mm spheres");int x=i%8,y=i/8;b.position_m={(x-3.5)*.06,(y<2?-1.:1.)*(y%2==0?.03001:.09001),.02005};b.type=x>=4?1:0;}
w.bodies.push_back(b);}
if(c.kind=="packing"||c.kind=="mixing"){FixedBox floor;floor.id=1;floor.centre_m={0,0,-.05};floor.half_m={.5,.5,.05};w.meshes.emplace_back(triangulated_box(floor));}
if(c.kind=="discharge"){FixedBox side;side.id=2;side.centre_m={.32,0,0};side.half_m={.01,.4,.2};w.boxes.push_back(side);side.id=3;side.centre_m.x=-.32;w.boxes.push_back(side);}
if(c.kind=="mixing"){FixedBox p;p.id=2;p.centre_m={0,0,.04};p.half_m={.24,.01,.03};PrescribedMotion motion;motion.pivot_m=p.centre_m;motion.rpm=c.rpm;w.moving_meshes.emplace_back(StaticTriangleMesh(triangulated_box(p)),motion);}
return w;}
std::array<double,2> masses(const ClumpWorld& w){std::array<double,2> v{};for(const auto& b:w.bodies)v[b.type]+=clump_properties(b.pebbles,b.density_kg_m3).mass_kg;return v;}
}
int main(int argc,char** argv){try{
check(argc==4||argc==5,"usage: powder_opt01 INPUT.bench NEW_DIRECTORY profile_0_or_1 [cached|reference|grid|reuse|walls]");
const std::string path=argc==5?argv[4]:"cached";check(path=="cached"||path=="reference"||path=="grid"||path=="reuse"||path=="walls","solver path");std::string flag=argv[3];check(flag=="0"||flag=="1","profile flag");bool enabled=flag=="1";const auto cases=read(argv[1]);std::filesystem::path out=argv[2];check(std::filesystem::create_directory(out),"output must be new");std::ifstream source(argv[1]);write(out/"input.bench",std::string(std::istreambuf_iterator<char>(source),{}));
auto physics=output(out/"physics.csv","case,status,requested_count,accepted_steps,physical_s,initial_mass_a_kg,initial_mass_b_kg,max_mass_residual_kg,pair_contacts,wall_contacts,max_overlap_radius_ratio,kinetic_energy_j,wall_work_j,mean_wall_torque_z_nm,arrived_count,arrived_mass_kg,mean_residence_s,median_residence_s,p95_residence_s,freefall_discrete_error_m,mixing_valid,mixing_index,error");
auto times=output(out/"timings.csv","case,profile_enabled,initialization_s,solver_call_s,measurement_and_collection_s,loop_s,output_s,physical_s,loop_rate");
auto phases=output(out/"profile.csv","case,phase,seconds,attempted_steps,accepted_steps,pair_distance_checks,post_pair_checks,material_pair_validations,neighbor_rebuilds,neighbor_reuses,neighbor_capacity_bytes,workspace_capacity_bytes,wall_pose_builds,wall_triangle_checks,wall_analytic_queries,wall_bounds_rejects");
auto windows=output(out/"windows.csv","case,end_step,physical_interval_s,wall_interval_s,rate,active_count");bool all=true;
for(const auto& c:cases){if(c.count>300){physics<<c.name<<",unsupported,"<<c.count<<",0,0,0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,0,0,0,clump_body_limit_300\n";continue;}
const auto init_start=Clock::now();auto w=fixture(c);w.use_cached_sphere_path=path!="reference";w.use_spatial_search=path=="grid"||path=="reuse"||path=="walls";w.reuse_neighbor_list=path=="reuse"||path=="walls";w.use_prepared_walls=path=="walls";const auto initial=w;const auto initial_mass=masses(w);const auto initial_text=serialize_clump_state(w);const double init_time=elapsed(init_start);write(out/(c.name+"_initial.csv"),initial_text);
ClumpProfile profile;std::array<double,2> exited_mass{};std::vector<double> residence;std::ostringstream events;events<<std::setprecision(17)<<"id,group,crossing_time_s,removal_time_s,mass_kg\n";
std::uint64_t pair_contacts=0,wall_contacts=0;int accepted=0;double maxoverlap=0,maxmass=0,torque_sum=0,solver_seconds=0,measure_seconds=0,window_physical=0;std::string error;auto loop_start=Clock::now(),window_start=loop_start;
try{for(int step=0;step<c.steps;++step){std::vector<ClumpBody> before;auto measure_start=Clock::now();if(c.kind=="discharge")before=w.bodies;measure_seconds+=elapsed(measure_start);auto step_start=Clock::now();const auto r=w.step_profiled(c.dt,enabled?&profile:nullptr);solver_seconds+=elapsed(step_start);++accepted;measure_start=Clock::now();pair_contacts+=r.pair_contacts;wall_contacts+=r.wall_contacts;maxoverlap=std::max(maxoverlap,r.max_overlap_m/c.radius);if(!r.moving_wall_loads.empty())torque_sum+=r.moving_wall_loads[0].reaction_torque_nm.z;
if(c.kind=="discharge"){std::vector<ClumpBody> remaining;for(const auto& b:w.bodies){if(b.position_m.z<0){auto old=std::find_if(before.begin(),before.end(),[&](const auto& v){return v.id==b.id;});check(old!=before.end()&&old->position_m.z>=0,"invalid discharge crossing");double frac=old->position_m.z/(old->position_m.z-b.position_m.z);double crossing=(step+frac)*c.dt;double mass=clump_properties(b.pebbles,b.density_kg_m3).mass_kg;exited_mass[b.type]+=mass;residence.push_back(crossing);events<<b.id<<','<<b.type<<','<<crossing<<','<<w.physical_time_s<<','<<mass<<'\n';}else remaining.push_back(b);}w.bodies=std::move(remaining);}
if((step+1)%c.window==0){auto active=masses(w);for(int g=0;g<2;++g)maxmass=std::max(maxmass,std::abs(initial_mass[g]-active[g]-exited_mass[g]));double wall=elapsed(window_start);windows<<c.name<<','<<step+1<<','<<w.physical_time_s-window_physical<<','<<wall<<','<<(w.physical_time_s-window_physical)/wall<<','<<w.bodies.size()<<'\n';window_physical=w.physical_time_s;window_start=Clock::now();}measure_seconds+=elapsed(measure_start);
}}catch(const std::exception& e){error=e.what();}
const double loop_time=elapsed(loop_start);auto output_start=Clock::now();double energy=0,freefall_error=0;for(const auto& b:w.bodies)energy+=clump_kinetic_energy(b);
if(c.kind=="freefall"){for(std::size_t i=0;i<w.bodies.size();++i){double expected=initial.bodies[i].position_m.z-9.81*c.dt*c.dt*accepted*(accepted+1.)/2;freefall_error=std::max(freefall_error,std::abs(w.bodies[i].position_m.z-expected));}}
bool mixvalid=false;double mixindex=0;if(c.kind=="mixing"){MixingConfig mc;mc.count_a=mc.count_b=16;mc.density_a=mc.density_b=c.density;mc.dt=c.dt;mc.duration_s=c.dt*c.steps;mc.sample_s=c.dt*c.window;std::map<ParticleId,unsigned> groups;for(const auto& b:w.bodies)groups.emplace(b.id,b.type);auto sample=measure_mixing(mc,w.bodies,groups,static_cast<std::uint64_t>(accepted));mixvalid=sample.valid;mixindex=sample.index;}
const double total_mass=initial_mass[0]+initial_mass[1];bool passed=error.empty()&&accepted==c.steps&&maxmass<=1e-12*total_mass+1e-18&&std::isfinite(energy)&&maxoverlap<.05;
if(c.kind=="freefall")passed=passed&&pair_contacts==0&&wall_contacts==0&&freefall_error<1e-11;
if(c.kind=="packing")passed=passed&&pair_contacts>0&&wall_contacts>0;
if(c.kind=="mixing")passed=passed&&wall_contacts>0&&mixvalid;
if(c.kind=="discharge")passed=passed&&residence.size()==static_cast<std::size_t>(c.count)&&w.bodies.empty();
const bool expected_rejection=c.expected=="reject_overlap"&&error.empty()&&accepted==c.steps&&maxoverlap>=.05&&maxmass<=1e-12*total_mass+1e-18;
if(!passed&&error.empty())error=expected_rejection?"overlap_above_predeclared_0.05":"baseline acceptance failed";
const bool case_ok=c.expected=="pass"?passed:expected_rejection;all=all&&case_ok;
std::sort(residence.begin(),residence.end());double mean=-1,median=-1,p95=-1;if(!residence.empty()){mean=std::accumulate(residence.begin(),residence.end(),0.)/residence.size();median=residence[(residence.size()-1)/2];p95=residence[static_cast<std::size_t>(std::ceil(.95*residence.size()))-1];}
physics<<c.name<<','<<(expected_rejection?"reference_rejected":passed?"passed":"failed")<<','<<c.count<<','<<accepted<<','<<w.physical_time_s<<','<<initial_mass[0]<<','<<initial_mass[1]<<','<<maxmass<<','<<pair_contacts<<','<<wall_contacts<<','<<maxoverlap<<','<<energy<<','<<w.moving_wall_work_j<<','<<(accepted?torque_sum/accepted:0)<<','<<residence.size()<<','<<exited_mass[0]+exited_mass[1]<<','<<mean<<','<<median<<','<<p95<<','<<freefall_error<<','<<mixvalid<<','<<mixindex<<','<<error<<'\n';
write(out/(c.name+"_final.csv"),serialize_clump_state(w));write(out/(c.name+"_events.csv"),events.str());
for(std::size_t i=0;i<profile.seconds.size();++i)phases<<c.name<<','<<ClumpProfile::names[i]<<','<<profile.seconds[i]<<','<<profile.attempted_steps<<','<<profile.accepted_steps<<','<<profile.pair_distance_checks<<','<<profile.post_pair_checks<<','<<profile.material_pair_validations<<','<<profile.neighbor_rebuilds<<','<<profile.neighbor_reuses<<','<<profile.neighbor_capacity_bytes<<','<<profile.workspace_capacity_bytes<<','<<profile.wall_pose_builds<<','<<profile.wall_triangle_checks<<','<<profile.wall_analytic_queries<<','<<profile.wall_bounds_rejects<<'\n';
const double output_time=elapsed(output_start);times<<c.name<<','<<enabled<<','<<init_time<<','<<solver_seconds<<','<<measure_seconds<<','<<loop_time<<','<<output_time<<','<<w.physical_time_s<<','<<w.physical_time_s/loop_time<<'\n';std::cout<<c.name<<' '<<(case_ok?(expected_rejection?"EXPECTED_REFERENCE_REJECTION":"PASS"):"FAIL")<<" rate="<<w.physical_time_s/loop_time<<" error="<<error<<'\n';
}
for(auto* f:{&physics,&times,&phases,&windows}){f->close();check(bool(*f),"CSV close failed");}write(out/"result.json",std::string("{\"schema\":1,\"suite\":\"optimization01_baseline\",\"passed\":")+(all?"true":"false")+",\"profile_enabled\":"+(enabled?"true":"false")+",\"large_scale_target_supported\":false}\n");return all?0:3;
}catch(const std::exception& e){std::cerr<<"OPT01_REJECTED: "<<e.what()<<'\n';return 2;}}
