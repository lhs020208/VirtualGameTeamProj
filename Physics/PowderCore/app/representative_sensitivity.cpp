#include "powder/representative_runtime.hpp"
#include "powder/representative_applicability.hpp"
#include "powder/mixing.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace powder;
void check(bool ok,const char* message){if(!ok)throw std::invalid_argument(message);}
unsigned integer(const char* s){std::size_t used=0;const std::string text=s;auto n=std::stoul(text,&used);check(used==text.size()&&!text.empty()&&text[0]!='-'&&n<=1000000,"invalid integer");return static_cast<unsigned>(n);}
double jitter(unsigned seed,unsigned k,unsigned axis){std::uint32_t n=seed*747796405u+k*2891336453u+axis*277803737u;n^=n>>16;n*=2246822519u;n^=n>>13;return (static_cast<double>(n%2001)/1000-1)*.001;}
int main(int argc,char** argv){try{
 check(argc==7,"usage: powder_rep13 mixing|aperture_small|aperture_large Q A_QUARTERS SEED DT_DIVISOR NEW_DIRECTORY");
 const std::string scenario=argv[1];const bool mix=scenario=="mixing";check(mix||scenario=="aperture_small"||scenario=="aperture_large","unknown scenario");
 const unsigned q=integer(argv[2]),quarters=integer(argv[3]),seed=integer(argv[4]),divisor=integer(argv[5]);check(q==1||q==2||q==4||q==8,"q must be 1/2/4/8");check(quarters>=1&&quarters<=3&&seed<=1000&&(divisor==1||divisor==2),"invalid fixture controls");
 if(!mix)check(q==1,"aperture/segregation scopes forbid coarse execution");
 const double dt=5e-7/divisor,duration=mix?.12:.3,density=2500;
 const unsigned steps=static_cast<unsigned>(std::llround(duration/dt)),stride=steps/20;
 MaterialContact material;material.law=ContactLaw::Linear;material.linear={10000,.2,2500,.05,.1};
 const double diameter=mix||scenario=="aperture_small"?.01:.014;
 const std::vector<PhysicalSizeBin> bins=mix?std::vector<PhysicalSizeBin>{{0,0,diameter,density,32*quarters},{1,0,diameter,density,128-32*quarters}}:std::vector<PhysicalSizeBin>{{0,0,diameter,density,1}};
 const auto inventory=representative_inventory(bins,q);std::vector<RepresentativePlacement> placements;std::map<ParticleId,unsigned> cohorts;
 if(mix){
  const unsigned n=8/q,nx=n>=2?2:1,ny=n>=4?2:1,nz=n>=8?2:1;const double pitch=diameter*representative_length_scale(q)*1.05;
  for(unsigned block=0;block<16;++block){const unsigned x=block%8,y=block/8,g=x<2*quarters?0:1;const Vec3 centre{(x-3.5)*.04+jitter(seed,block,0),(y-.5)*.04+(g?.02:0)+jitter(seed,block,1),.016+jitter(seed,block,2)};
   for(unsigned j=0;j<n;++j){double ox=(static_cast<double>(j%nx)-(nx-1)*.5)*pitch,oy=(static_cast<double>((j/nx)%ny)-(ny-1)*.5)*pitch;const double oz=(static_cast<double>(j/(nx*ny))-(nz-1)*.5)*pitch;if(seed%2)std::swap(ox,oy);
    const ParticleId id=placements.size()+1;placements.push_back({id,g,0,{centre.x+ox,centre.y+oy,centre.z+oz},{g?-.5:.5,0,0}});cohorts[id]=block;
   }
  }
 }else placements.push_back({1,0,0,{0,0,.02},{}});
 std::map<std::pair<unsigned,unsigned>,MaterialContact> pairs{{{0,0},material}};std::map<unsigned,MaterialContact> walls{{0,material}};
 if(mix){pairs[{0,1}]=material;pairs[{1,1}]=material;walls[1]=material;}
 auto world=representative_world(bins,q,placements,pairs,walls,mix?RepresentativeScope::DryBulkCandidate:RepresentativeScope::ClearanceOrClogging);
 if(mix){FixedBox floor;floor.id=1;floor.centre_m={0,0,-.02};floor.half_m={.5,.5,.02};world.boxes.push_back(floor);}
 else for(int sign:{-1,1}){FixedBox wall;wall.id=sign<0?1:2;wall.centre_m={sign*.056,0,-.005};wall.half_m={.05,.2,.005};world.boxes.push_back(wall);}
 const std::filesystem::path out(argv[6]);check(std::filesystem::create_directory(out),"output must be new");
 auto file=[&](const char* name){std::ofstream f(out/name);check(bool(f),"open output failed");f<<std::setprecision(17);return f;};
 auto inv=file("inventory.csv");inv<<representative_inventory_csv(inventory);auto initial=file("initial.csv");initial<<serialize_clump_state(world);
 auto meta=file("metadata.csv");meta<<"case,q,a_quarters,seed,dt_s,duration_s,physical_count,computed_count,total_mass_kg,physical_kn,computed_kn,physical_cn,computed_cn,cohorts\n";const auto contact=representative_contact(material,q,RepresentativeScope::DryBulkCandidate);meta<<scenario<<','<<q<<','<<quarters<<','<<seed<<','<<dt<<','<<duration<<','<<inventory.physical_count<<','<<inventory.computational_count<<','<<inventory.total_mass_kg<<",10000,"<<contact.computational.linear.kn<<",0.2,"<<contact.computational.linear.cn<<','<<(mix?16:1)<<'\n';
 auto series=file("series.csv");series<<"sample,time_s,com_z,mass_a_kg,mass_b_kg,uniformity2,valid2,status2,uniformity4,valid4,status4,passed_mass_kg,contacts\n";
 auto cells=file("cells.csv");cells<<"sample,grid_x,cell,samples_a,samples_b,mass_a_kg,mass_b_kg\n";
 std::size_t contacts=0;double maximum_overlap=0;bool passed=false;
 MixingConfig config;config.dt=dt;config.duration_s=duration;config.sample_s=duration/20;config.lower={-.22,-.5,-1};config.upper={.22,.5,.5};config.grid={2,1,1};config.minimum_bodies=4;
 auto sample=[&](unsigned index){std::array<Vec3,16> sums{};std::array<double,16> weights{};std::array<unsigned,16> types{};std::array<double,2> mass{};double com=0;
  for(const auto& b:world.bodies){const double m=clump_properties(b.pebbles,b.density_kg_m3).mass_kg;mass[b.type]+=m;com+=m*b.position_m.z;if(mix){const auto k=cohorts.at(b.id);weights[k]+=m;types[k]=b.type;sums[k].x+=m*b.position_m.x;sums[k].y+=m*b.position_m.y;sums[k].z+=m*b.position_m.z;}}
  MixingSample two,four;
  if(mix){std::vector<ClumpBody> observations;std::map<ParticleId,unsigned> labels;
   for(unsigned k=0;k<16;++k){auto b=make_clump(k+1,types[k],{{{},.02}},density);b.position_m={sums[k].x/weights[k],sums[k].y/weights[k],sums[k].z/weights[k]};observations.push_back(b);labels[b.id]=b.type;}
   config.grid={2,1,1};config.minimum_bodies=4;two=measure_mixing(config,observations,labels,index*stride);config.grid={4,1,1};config.minimum_bodies=2;four=measure_mixing(config,observations,labels,index*stride);
   for(const auto* value:{&two,&four})for(std::size_t k=0;k<value->cells.size();++k){const auto& c=value->cells[k];cells<<index<<','<<value->cells.size()<<','<<k<<','<<c.count[0]<<','<<c.count[1]<<','<<c.mass[0]<<','<<c.mass[1]<<'\n';}
  }
  series<<index<<','<<index*duration/20<<','<<com/inventory.total_mass_kg<<','<<mass[0]<<','<<mass[1]<<',';if(two.valid)series<<two.index;series<<','<<two.valid<<','<<two.status<<',';if(four.valid)series<<four.index;series<<','<<four.valid<<','<<four.status<<','<<(passed?inventory.total_mass_kg:0)<<','<<contacts<<'\n';
 };
 sample(0);const auto start=std::chrono::steady_clock::now();for(unsigned step=1;step<=steps;++step){const auto report=world.step(dt);contacts+=report.pair_contacts+report.wall_contacts;maximum_overlap=std::max(maximum_overlap,report.max_overlap_m);if(!mix&&world.bodies[0].position_m.z<-.02)passed=true;if(step%stride==0)sample(step/stride);}
 const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();auto summary=file("summary.csv");summary<<"wall_s,rate,max_overlap_radius_ratio,contacts,passed,final_z\n"<<seconds<<','<<duration/seconds<<','<<maximum_overlap/(diameter*representative_length_scale(q)/2)<<','<<contacts<<','<<passed<<','<<world.bodies[0].position_m.z<<'\n';auto final=file("final.csv");final<<serialize_clump_state(world);
 for(auto* f:{&inv,&initial,&meta,&series,&cells,&summary,&final}){f->close();check(bool(*f),"write output failed");}
 std::cout<<scenario<<" q="<<q<<" fraction="<<quarters<<" seed="<<seed<<" divisor="<<divisor<<" overlap="<<maximum_overlap/(diameter*representative_length_scale(q)/2)<<" seconds="<<seconds<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
