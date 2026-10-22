#include "powder/representative_runtime.hpp"
#include "powder/representative_applicability.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace powder;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(int argc,char** argv){try{
 check(argc==3,"usage: powder_aperture13 bridge|psd NEW_DIRECTORY");const std::string mode=argv[1];check(mode=="bridge"||mode=="psd","unknown case");const bool psd=mode=="psd";
 MaterialContact m;m.law=ContactLaw::Linear;m.linear={10000,.2,2500,.05,.1};
 std::vector<PhysicalSizeBin> bins=psd?std::vector<PhysicalSizeBin>{{0,0,.01,2500,1},{1,0,.014,2500,1}}:std::vector<PhysicalSizeBin>{{0,0,.01,2500,2}};
 std::vector<RepresentativePlacement> positions=psd?std::vector<RepresentativePlacement>{{1,0,0,{0,-.04,.02},{}},{2,1,0,{0,.04,.02},{}}}:std::vector<RepresentativePlacement>{{1,0,0,{-.00525,0,.015},{}},{2,0,0,{.00525,0,.015},{}}};
 std::map<std::pair<unsigned,unsigned>,MaterialContact> pairs{{{0,0},m}};std::map<unsigned,MaterialContact> walls{{0,m}};if(psd){pairs[{0,1}]=m;pairs[{1,1}]=m;walls[1]=m;}
 auto world=representative_world(bins,1,positions,pairs,walls,psd?RepresentativeScope::SizeSegregation:RepresentativeScope::ClearanceOrClogging);
 for(int sign:{-1,1}){FixedBox wall;wall.id=sign<0?1:2;wall.centre_m={sign*.056,0,-.005};wall.half_m={.05,.2,.005};world.boxes.push_back(wall);}
 const std::filesystem::path out(argv[2]);check(std::filesystem::create_directory(out),"output must be new");
 std::ofstream series(out/"series.csv"),summary(out/"summary.csv"),gates(out/"geometry.csv");check(bool(series)&&bool(summary)&&bool(gates),"file open failed");for(auto* f:{&series,&summary,&gates})*f<<std::setprecision(17);
 series<<"time_s,passed_count,passed_mass_a_kg,passed_mass_b_kg,z1,z2,energy_j,contacts\n";std::set<ParticleId> passed;std::array<double,2> mass{};std::size_t contacts=0;double overlap=0;
 auto sample=[&](unsigned step){double energy=0;for(const auto& b:world.bodies)energy+=clump_kinetic_energy(b);series<<step*5e-7<<','<<passed.size()<<','<<mass[0]<<','<<mass[1]<<','<<world.bodies[0].position_m.z<<','<<world.bodies[1].position_m.z<<','<<energy<<','<<contacts<<'\n';};sample(0);
 for(unsigned step=1;step<=600000;++step){auto r=world.step(5e-7);contacts+=r.pair_contacts+r.wall_contacts;overlap=std::max(overlap,r.max_overlap_m/.005);for(const auto& b:world.bodies)if(b.position_m.z<-.02&&passed.insert(b.id).second)mass[b.type]+=clump_properties(b.pebbles,b.density_kg_m3).mass_kg;if(step%30000==0)sample(step);}
 gates<<"q,physical_diameter_m,computed_diameter_m,opening_m,physical_fits,computed_fits,changes_passability,scope_rejected\n";
 for(unsigned q:{1,2,4,8})for(const auto& bin:bins){auto c=representative_clearance(bin.diameter_m,.012,q);bool rejected=false;try{representative_contact(m,q,psd?RepresentativeScope::SizeSegregation:RepresentativeScope::ClearanceOrClogging);}catch(const std::invalid_argument&){rejected=true;}check(rejected==(q>1),"forbidden scope not enforced");gates<<q<<','<<c.physical_diameter_m<<','<<c.computed_diameter_m<<",0.012,"<<c.physical_fits<<','<<c.computed_fits<<','<<c.changes_passability<<','<<rejected<<'\n';}
 summary<<"case,passed_count,passed_mass_a_kg,passed_mass_b_kg,max_overlap_radius_ratio,contacts,physical_count,computed_count,q,duration_s\n"<<mode<<','<<passed.size()<<','<<mass[0]<<','<<mass[1]<<','<<overlap<<','<<contacts<<",2,2,1,0.3\n";
 for(auto* f:{&series,&summary,&gates}){f->close();check(bool(*f),"write failure");}std::cout<<mode<<" passed="<<passed.size()<<" overlap="<<overlap<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
