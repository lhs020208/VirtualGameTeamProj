#include "powder/settings.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
namespace powder {
namespace {
constexpr std::uint64_t max_particles=300;
void require(bool value,const char* reason){if(!value)throw std::invalid_argument(reason);}
void finite(double x){require(std::isfinite(x),"settings contain nonfinite value");}
void finite(Vec3 v){finite(v.x);finite(v.y);finite(v.z);}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
MaterialContact contact(const PowderSettings& s,const ContactSettings& c,bool wall){
 MaterialContact m;m.law=s.law;m.linear=c.linear;m.a=s.particle_material;m.b=wall?s.wall_material:s.particle_material;
 m.cn=c.cn;m.ct=c.ct;m.mu=c.mu;m.max_overlap_radius_ratio=c.overlap_cap;m.rolling=c.rolling;m.adhesion_gamma_j_m2=c.adhesion_gamma_j_m2;return m;
}
struct Random {
 std::mt19937_64 engine;
 explicit Random(std::uint64_t seed):engine(seed){}
 double unit(){return static_cast<double>(engine()>>11)*0x1.0p-53;}
 std::uint64_t index(std::uint64_t n){const auto threshold=(std::uint64_t(0)-n)%n;for(;;){auto r=engine();if(r>=threshold)return r%n;}}
};
void expect(std::istream& in,const char* token){std::string s;require(bool(in>>s)&&s==token,"settings schema/key/order mismatch");}
std::uint64_t unsigned_value(std::istream& in){std::string s;require(bool(in>>s),"missing settings integer");std::uint64_t n=0;auto r=std::from_chars(s.data(),s.data()+s.size(),n);require(r.ec==std::errc{}&&r.ptr==s.data()+s.size(),"invalid settings integer");return n;}
bool clear_of_walls(const Particle& p,const ParticleSystem& w,double gap){
 const double radius=p.diameter_m/2+gap;
 for(auto& plane:w.planes)if(dot(p.position_m,plane.normal)-plane.offset_m<radius)return false;
 for(auto& box:w.boxes){
  Vec3 d{p.position_m.x-box.centre_m.x,p.position_m.y-box.centre_m.y,p.position_m.z-box.centre_m.z};
  const std::array<double,3> h{box.half_m.x,box.half_m.y,box.half_m.z};double distance2=0;
  for(int k=0;k<3;++k){double q=std::max(0.0,std::abs(dot(d,box.axes[k]))-h[k]);distance2+=q*q;}
  if(distance2<radius*radius)return false;
 }return true;
}
}
void validate_settings(const PowderSettings& s){
 require(s.supply_mode==SupplyMode::Batch||s.supply_mode==SupplyMode::Timed,"unknown supply mode");
 finite(s.supply_start_s);finite(s.supply_interval_s);finite(s.retry_interval_s);
 require(s.supply_start_s>=0&&s.supply_interval_s>0&&s.retry_interval_s>0,"invalid supply times");
 require(s.supply_per_event>=1&&s.supply_per_event<=300,"supply per event must be 1..300");
 require(s.supply_start_s+300*s.supply_interval_s<=1e12,"supply schedule exceeds supported time");
 require(s.distribution==SizeDistribution::Mono||s.distribution==SizeDistribution::UniformNumber,"unknown size distribution");
 finite(s.diameter_min_m);finite(s.diameter_max_m);
 require(s.diameter_min_m>0&&s.diameter_max_m>=s.diameter_min_m,"invalid diameter bounds");
 require(s.distribution!=SizeDistribution::Mono||s.diameter_min_m==s.diameter_max_m,"mono diameters must match");
 (void)sphere_properties(s.diameter_min_m,{s.density_kg_m3});(void)sphere_properties(s.diameter_max_m,{s.density_kg_m3});
 finite(s.target_mass_kg);finite(s.mass_tolerance_kg);
 if(s.amount==AmountMode::Count)require(s.count>=1&&s.count<=max_particles&&s.target_mass_kg==0&&s.mass_tolerance_kg==0,"count mode requires 1..300 and no mass request");
 else if(s.amount==AmountMode::Mass)require(s.count==0&&s.target_mass_kg>0&&s.mass_tolerance_kg>=0&&s.mass_tolerance_kg<s.target_mass_kg,"mass mode requires count=0, positive mass and tolerance below target");
 else throw std::invalid_argument("unknown amount mode");
 require(s.seed<=std::uint64_t(std::numeric_limits<std::int64_t>::max()),"seed exceeds supported signed-64 range");
 require(s.max_candidates>=1&&s.max_candidates<=1000000,"candidate budget must be 1..1000000");
 finite(s.centre_m);finite(s.half_m);finite(s.velocity_m_s);finite(s.gravity_m_s2);
 require(s.half_m.x>0&&s.half_m.y>0&&s.half_m.z>0,"spawn half extents must be positive");
 finite(s.spacing_factor);require(s.spacing_factor>1&&s.spacing_factor<=10,"spacing factor must be in (1,10]");
 finite(s.wall_clearance_m);require(s.wall_clearance_m>=0,"wall clearance must be nonnegative");
 finite(s.dt_s);require(s.dt_s>0,"dt must be positive");
 // Validate both stored model parameter sets, even when one is inactive.
 for(auto* c:{&s.pair,&s.wall})for(auto law:{ContactLaw::Linear,ContactLaw::HertzMindlin}){auto m=contact(s,*c,c==&s.wall);m.law=law;validate_contact_model(m);}
 require(s.law==ContactLaw::Linear||s.law==ContactLaw::HertzMindlin,"unknown contact law");
}
std::string serialize_settings(const PowderSettings& s){
 validate_settings(s);std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17);
 out<<"POWDER_SETTINGS 4\ngenerator lattice_v1\nsize "<<(s.distribution==SizeDistribution::Mono?"mono":"uniform_number")<<' '<<s.diameter_min_m<<' '<<s.diameter_max_m<<"\ndensity_kg_m3 "<<s.density_kg_m3<<"\namount ";
 if(s.amount==AmountMode::Count)out<<"count "<<s.count;else out<<"mass "<<s.target_mass_kg<<' '<<s.mass_tolerance_kg;
 out<<"\nseed "<<s.seed<<"\nlaw "<<(s.law==ContactLaw::Linear?"linear":"hertz_mindlin")<<"\nparticle_elastic "<<s.particle_material.young_pa<<' '<<s.particle_material.poisson<<"\nwall_elastic "<<s.wall_material.young_pa<<' '<<s.wall_material.poisson;
 auto material=[&](const char* name,const ContactSettings& c){out<<'\n'<<name<<"_linear "<<c.linear.kn<<' '<<c.linear.cn<<' '<<c.linear.kt<<' '<<c.linear.ct<<' '<<c.linear.mu;out<<'\n'<<name<<"_hertz "<<c.cn<<' '<<c.ct<<' '<<c.mu<<' '<<c.overlap_cap;};material("pair",s.pair);material("wall",s.wall);
 auto vector=[&](const char* name,Vec3 v){out<<'\n'<<name<<' '<<v.x<<' '<<v.y<<' '<<v.z;};vector("centre_m",s.centre_m);vector("half_m",s.half_m);vector("velocity_m_s",s.velocity_m_s);vector("gravity_m_s2",s.gravity_m_s2);
 out<<"\nspacing_factor "<<s.spacing_factor<<"\nwall_clearance_m "<<s.wall_clearance_m<<"\nmax_candidates "<<s.max_candidates<<"\ndt_s "<<s.dt_s<<'\n';out<<"supply "<<(s.supply_mode==SupplyMode::Batch?"batch":"timed")<<' '<<s.supply_start_s<<' '<<s.supply_interval_s<<' '<<s.supply_per_event<<' '<<s.retry_interval_s<<'\n';
 auto rolling=[&](const char* name,const RollingModel& m){out<<name<<' '<<m.stiffness_n_m<<' '<<m.damping_ns_m<<' '<<m.mu<<'\n';};rolling("pair_rolling_sds",s.pair.rolling);rolling("wall_rolling_sds",s.wall.rolling);
 out<<"contact_adhesion_gamma_j_m2 "<<s.pair.adhesion_gamma_j_m2<<' '<<s.wall.adhesion_gamma_j_m2<<'\n';return out.str();
}
PowderSettings parse_settings(const std::string& text){
 require(text.size()<=65536,"settings file exceeds 64 KiB");std::istringstream in(text);in.imbue(std::locale::classic());PowderSettings s;
 expect(in,"POWDER_SETTINGS");const auto version=unsigned_value(in);require(version>=1&&version<=4,"unsupported settings schema version");expect(in,"generator");expect(in,"lattice_v1");expect(in,"size");std::string name;in>>name;
 if(name=="mono")s.distribution=SizeDistribution::Mono;else if(name=="uniform_number")s.distribution=SizeDistribution::UniformNumber;else throw std::invalid_argument("unknown size distribution");
 in>>s.diameter_min_m>>s.diameter_max_m;expect(in,"density_kg_m3");in>>s.density_kg_m3;expect(in,"amount");in>>name;
 if(name=="count")s.count=unsigned_value(in);else if(name=="mass"){s.amount=AmountMode::Mass;s.count=0;in>>s.target_mass_kg>>s.mass_tolerance_kg;}else throw std::invalid_argument("unknown amount mode");
 expect(in,"seed");s.seed=unsigned_value(in);expect(in,"law");in>>name;
 if(name=="linear")s.law=ContactLaw::Linear;else if(name=="hertz_mindlin")s.law=ContactLaw::HertzMindlin;else throw std::invalid_argument("unknown contact law");
 expect(in,"particle_elastic");in>>s.particle_material.young_pa>>s.particle_material.poisson;expect(in,"wall_elastic");in>>s.wall_material.young_pa>>s.wall_material.poisson;
 auto material=[&](const char* linear,const char* hertz,ContactSettings& c){expect(in,linear);in>>c.linear.kn>>c.linear.cn>>c.linear.kt>>c.linear.ct>>c.linear.mu;expect(in,hertz);in>>c.cn>>c.ct>>c.mu>>c.overlap_cap;};material("pair_linear","pair_hertz",s.pair);material("wall_linear","wall_hertz",s.wall);
 auto vector=[&](const char* key,Vec3& v){expect(in,key);in>>v.x>>v.y>>v.z;};vector("centre_m",s.centre_m);vector("half_m",s.half_m);vector("velocity_m_s",s.velocity_m_s);vector("gravity_m_s2",s.gravity_m_s2);
 expect(in,"spacing_factor");in>>s.spacing_factor;expect(in,"wall_clearance_m");in>>s.wall_clearance_m;expect(in,"max_candidates");s.max_candidates=unsigned_value(in);expect(in,"dt_s");in>>s.dt_s;
 if(version>=2){expect(in,"supply");in>>name;if(name=="batch")s.supply_mode=SupplyMode::Batch;else if(name=="timed")s.supply_mode=SupplyMode::Timed;else throw std::invalid_argument("unknown supply mode");in>>s.supply_start_s>>s.supply_interval_s;s.supply_per_event=unsigned_value(in);in>>s.retry_interval_s;}
 if(version>=3){expect(in,"pair_rolling_sds");in>>s.pair.rolling.stiffness_n_m>>s.pair.rolling.damping_ns_m>>s.pair.rolling.mu;expect(in,"wall_rolling_sds");in>>s.wall.rolling.stiffness_n_m>>s.wall.rolling.damping_ns_m>>s.wall.rolling.mu;}
 if(version>=4){expect(in,"contact_adhesion_gamma_j_m2");in>>s.pair.adhesion_gamma_j_m2>>s.wall.adhesion_gamma_j_m2;}
 require(!in.fail(),"invalid or truncated settings value");std::string extra;require(!(in>>extra),"unexpected trailing settings token");validate_settings(s);return s;
}
InitialBatch make_inventory(const PowderSettings& s,ParticleSystem world){
 validate_settings(s);validate_boxes(world.boxes);
 for(auto& p:world.planes){finite(p.normal);finite(p.offset_m);require(std::abs(std::sqrt(dot(p.normal,p.normal))-1)<1e-12,"initial plane normal must be unit");}
 world.particles.clear();world.reset_history();world.model=contact(s,s.pair,false);world.wall_model=contact(s,s.wall,true);world.gravity_m_s2=s.gravity_m_s2;
 InitialReport report;report.configured=true;report.seed=s.seed;report.target_mass_kg=s.target_mass_kg;report.min_diameter_m=std::numeric_limits<double>::max();
 Random sizes(s.seed);std::vector<Particle> particles;
 const auto count=s.amount==AmountMode::Count?s.count:max_particles;
 for(std::uint64_t i=0;i<count;++i){Particle p;p.id=i+1;p.diameter_m=s.diameter_min_m;
  if(s.distribution==SizeDistribution::UniformNumber)p.diameter_m+=(s.diameter_max_m-s.diameter_min_m)*sizes.unit();
  p.material.density_kg_m3=s.density_kg_m3;p.velocity_m_s=s.velocity_m_s;const double m=sphere_properties(p.diameter_m,p.material).mass_kg;
  if(s.amount==AmountMode::Mass&&report.mass_kg+m>=s.target_mass_kg){
   if(particles.empty()||report.mass_kg+m-s.target_mass_kg<s.target_mass_kg-report.mass_kg){particles.push_back(p);report.mass_kg+=m;}break;
  }
  particles.push_back(p);report.mass_kg+=m;
 }
 finite(report.mass_kg);report.count=particles.size();require(!particles.empty(),"mass request produced no complete particle");
 if(s.amount==AmountMode::Mass){report.mass_error_kg=report.mass_kg-s.target_mass_kg;if(std::abs(report.mass_error_kg)>s.mass_tolerance_kg){std::ostringstream out;out<<std::setprecision(17)<<"mass request cannot be met by nearest seeded prefix (limit 300): requested="<<s.target_mass_kg<<" actual="<<report.mass_kg<<" error="<<report.mass_error_kg<<" tolerance="<<s.mass_tolerance_kg;throw std::invalid_argument(out.str());}}
 for(auto& p:particles){report.min_diameter_m=std::min(report.min_diameter_m,p.diameter_m);report.max_diameter_m=std::max(report.max_diameter_m,p.diameter_m);}
 world.particles=std::move(particles);return {std::move(world),report};
}
std::size_t place_supply(const PowderSettings& s,ParticleSystem& world,const std::vector<Particle>& particles,double diameter,const std::function<bool()>& cancelled,std::uint64_t* attempts){
 if(particles.empty())return 0;
 const double pitch=diameter*s.spacing_factor;finite(pitch);
 std::array<double,3> half{s.half_m.x,s.half_m.y,s.half_m.z},centre{s.centre_m.x,s.centre_m.y,s.centre_m.z};
 std::array<std::uint64_t,3> cells{};std::uint64_t total=1;
 for(int k=0;k<3;++k){require(2*half[k]>=diameter,"spawn volume smaller than largest particle");double n=std::floor((2*half[k]-diameter)/pitch)+1;require(std::isfinite(n)&&n>=1&&n<=1e12,"spawn lattice axis exceeds supported range");cells[k]=static_cast<std::uint64_t>(n);require(total<=1000000000000ULL/cells[k],"spawn lattice exceeds 1e12 cells; tighten spawn volume");total*=cells[k];}

 Random positions(s.seed^0xd1b54a32d192ed03ULL);std::uint64_t cell=positions.index(total),stride=total>1?positions.index(total-1)+1:1;
 while(std::gcd(stride,total)!=1){if(++stride>=total)stride=1;}
 std::size_t placed=0;
 for(std::uint64_t attempt=0;attempt<std::min(total,s.max_candidates)&&placed<particles.size();++attempt){
  if(attempt%128==0&&cancelled&&cancelled())throw std::runtime_error("initial placement cancelled");
  if(attempts)++*attempts;
  auto p=particles[placed];auto code=cell;std::array<double,3> xyz{};
  for(int k=0;k<3;++k){auto index=code%cells[k];code/=cells[k];xyz[k]=centre[k]+(static_cast<double>(index)-static_cast<double>(cells[k]-1)/2)*pitch;}
  p.position_m=(s.amount==AmountMode::Count&&s.count==1)?s.centre_m:Vec3{xyz[0],xyz[1],xyz[2]};
  // Largest-diameter lattice separates all particles; independently check actual radii too.
  bool clear=clear_of_walls(p,world,s.wall_clearance_m);
  for(auto& q:world.particles)if(std::hypot(p.position_m.x-q.position_m.x,p.position_m.y-q.position_m.y,p.position_m.z-q.position_m.z)<(p.diameter_m+q.diameter_m)/2)clear=false;
  if(clear){world.particles.push_back(p);++placed;}cell=(cell+stride)%total;
  if(s.amount==AmountMode::Count&&s.count==1)break; // Single-particle mode means exactly the requested centre.
 }
 return placed;
}
InitialBatch make_initial(const PowderSettings& s,ParticleSystem world,const std::function<bool()>& cancelled){
 auto batch=make_inventory(s,std::move(world));auto particles=std::move(batch.world.particles);batch.world.particles.clear();
 auto placement=s;if(particles.size()==1){placement.amount=AmountMode::Count;placement.count=1;placement.target_mass_kg=placement.mass_tolerance_kg=0;}
 const auto placed=place_supply(placement,batch.world,particles,batch.report.max_diameter_m,cancelled,&batch.report.candidates);
 if(placed!=particles.size()){std::ostringstream out;out<<"initial placement failed: placed "<<placed<<" of "<<particles.size()<<"; enlarge/move spawn volume, reduce count/diameter or increase candidate budget. No partial batch committed";throw std::invalid_argument(out.str());}
 validate_particles(batch.world.particles);return batch;
}
}
