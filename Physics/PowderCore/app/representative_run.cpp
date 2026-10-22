#include "powder/representative_runtime.hpp"
#include "powder/mixing.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
using namespace powder;
void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
 require(argc==4,"usage: powder_rep12 packing|discharge|mixing 1|8 NEW_DIRECTORY");
 const std::string scenario=argv[1],ratio=argv[2];require(scenario=="packing"||scenario=="discharge"||scenario=="mixing","unknown case");require(ratio=="1"||ratio=="8","fixture ratio must be 1 or 8");const std::uint64_t q=ratio=="1"?1:8;
 const std::vector<PhysicalSizeBin> bins{{0,0,.01,2500,128},{1,0,.01,2500,128}};
 const auto inventory=representative_inventory(bins,q);MaterialContact material;material.law=ContactLaw::Linear;material.linear={1000,.1,250,.025,.4};
 std::vector<RepresentativePlacement> placements;std::map<ParticleId,unsigned> blocks;
 for(unsigned block=0;block<32;++block){const unsigned x=block%8,y=block/8,group=x>=4?1:0;const Vec3 centre{(x-3.5)*.035,(y-1.5)*.035+(scenario=="mixing"&&group?.0175:0),.06};
  for(unsigned j=0;j<8/q;++j){Vec3 p=centre;if(q==1){p.x+=(j%2?1:-1)*.0051;p.y+=((j/2)%2?1:-1)*.0051;p.z+=(j/4?1:-1)*.0051;}
   const auto id=static_cast<ParticleId>(placements.size()+1);placements.push_back({id,group,0,p,{scenario=="mixing"?(group?-1.2:1.2):0,0,0}});blocks[id]=block;
  }
 }
 auto world=representative_world(bins,q,placements,{{{0,0},material},{{0,1},material},{{1,1},material}},{{0,material},{1,material}});
 if(scenario!="discharge"){FixedBox floor;floor.id=1;floor.centre_m={0,0,-.02};floor.half_m={.4,.4,.02};world.boxes.push_back(floor);}
 else {for(int sign:{-1,1}){FixedBox side;side.id=sign<0?1:2;side.centre_m={sign*.16,0,.05};side.half_m={.01,.2,.15};world.boxes.push_back(side);}}
 const std::filesystem::path out(argv[3]);require(std::filesystem::create_directory(out),"output must be new");auto file=[&](const char* name){std::ofstream f(out/name);require(bool(f),"output open failed");f<<std::setprecision(17);return f;};
 auto inv=file("inventory.csv");inv<<representative_inventory_csv(inventory);auto initial=file("initial.csv");initial<<serialize_clump_state(world);
 auto metadata=file("metadata.csv");metadata<<"q,length_scale,physical_count,computational_count,total_mass_kg,dt_s,duration_s,physical_kn,computed_kn,physical_cn,computed_cn,matched_observation_samples,minimum_samples_per_cell\n";const auto model=representative_contact(material,q,RepresentativeScope::DryBulkCandidate);metadata<<q<<','<<inventory.bins[0].length_scale<<",256,"<<world.bodies.size()<<','<<inventory.total_mass_kg<<",0.000002,0.25,1000,"<<model.computational.linear.kn<<",0.1,"<<model.computational.linear.cn<<",32,4\n";
 auto series=file("series.csv");series<<"step,time_s,com_z,energy_j,arrived_physical_count,arrived_mass_kg,mass_a_kg,mass_b_kg,mixing_valid,mixing_index,sample_count,contacts\n";
 auto cells=file("cells.csv");cells<<"step,cell,samples_a,samples_b,mass_a_kg,mass_b_kg\n";
 std::set<ParticleId> arrived;std::size_t contacts=0;double maxoverlap=0;MixingConfig config;config.dt=.000002;config.duration_s=.25;config.sample_s=.0125;config.lower={-.4,-.4,-1};config.upper={.4,.4,.3};
 auto sample=[&](unsigned step){double com=0,energy=0,exitmass=0;std::array<double,2> mass{};std::array<Vec3,32> centres{};std::array<double,32> weights{};std::array<unsigned,32> types{};
  for(const auto& b:world.bodies){const double m=clump_properties(b.pebbles,b.density_kg_m3).mass_kg;mass[b.type]+=m;com+=m*b.position_m.z;energy+=clump_kinetic_energy(b);if(arrived.count(b.id))exitmass+=m;auto k=blocks.at(b.id);centres[k].x+=m*b.position_m.x;centres[k].y+=m*b.position_m.y;centres[k].z+=m*b.position_m.z;weights[k]+=m;types[k]=b.type;}
  std::vector<ClumpBody> observations;std::map<ParticleId,unsigned> groups;
  for(unsigned k=0;k<32;++k){auto b=make_clump(k+1,types[k],{{{},.02}},2500);b.position_m={centres[k].x/weights[k],centres[k].y/weights[k],centres[k].z/weights[k]};observations.push_back(b);groups[b.id]=b.type;}
  const auto mixing=measure_mixing(config,observations,groups,step);
  for(std::size_t k=0;k<mixing.cells.size();++k){const auto& c=mixing.cells[k];cells<<step<<','<<k<<','<<c.count[0]<<','<<c.count[1]<<','<<c.mass[0]<<','<<c.mass[1]<<'\n';}
  series<<step<<','<<step*config.dt<<','<<com/inventory.total_mass_kg<<','<<energy<<','<<arrived.size()*q<<','<<exitmass<<','<<mass[0]<<','<<mass[1]<<','<<mixing.valid<<',';if(mixing.valid)series<<mixing.index;series<<",32,"<<contacts<<'\n';
 };
 sample(0);const auto start=std::chrono::steady_clock::now();for(unsigned step=1;step<=125000;++step){const auto r=world.step(config.dt);contacts+=r.pair_contacts+r.wall_contacts;maxoverlap=std::max(maxoverlap,r.max_overlap_m);if(scenario=="discharge")for(const auto& b:world.bodies)if(b.position_m.z<0)arrived.insert(b.id);if(step%6250==0)sample(step);}
 const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();auto summary=file("summary.csv");summary<<"wall_s,rate,max_overlap_m,contacts,arrived_physical_count\n"<<elapsed<<','<<.25/elapsed<<','<<maxoverlap<<','<<contacts<<','<<arrived.size()*q<<'\n';auto final=file("final.csv");final<<serialize_clump_state(world);
 for(auto* f:{&inv,&initial,&metadata,&series,&cells,&summary,&final}){f->close();require(bool(*f),"output write failed");}std::cout<<scenario<<" q="<<q<<" rate="<<.25/elapsed<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
