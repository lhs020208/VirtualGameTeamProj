#include "powder/representative.hpp"
#include "powder/clump.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
using namespace powder;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"missing representative rejection");}
void near(double a,double b){check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=1e-24+1e-10*std::max(std::abs(a),std::abs(b)),"representative numerical mismatch");}
void vector_scaled(Vec3 a,Vec3 b,double s){near(a.x*s,b.x);near(a.y*s,b.y);near(a.z*s,b.z);}
MaterialContact model(){MaterialContact m;m.law=ContactLaw::Linear;m.linear={10000,.01,2500,.0025,.4};return m;}
Particle scale(Particle p,double s){p.diameter_m*=s;p.position_m={p.position_m.x*s,p.position_m.y*s,p.position_m.z*s};p.angular_velocity_rad_s={p.angular_velocity_rad_s.x/s,p.angular_velocity_rad_s.y/s,p.angular_velocity_rad_s.z/s};return p;}
int main(int argc,char** argv){try{
 const std::vector<PhysicalSizeBin> bins{{0,0,.002,1500,24},{0,1,.001,1500,48},{1,0,.003,2500,16},{1,1,.0015,2500,32}};
 auto identity=representative_inventory(bins,1);
 near(identity.bins[0].physical_particle_mass_kg,4./3*3.14159265358979323846*1e-9*1500);
 check(identity.computational_count==120&&identity.physical_count==120,"identity count");
 for(auto q:{1ull,2ull,8ull}){auto inv=representative_inventory(bins,q);check(inv.physical_count==120&&inv.computational_count==120/q&&inv.groups.size()==2,"integer/group conservation");near(inv.total_mass_kg,identity.total_mass_kg);
  for(const auto& g:inv.groups){check(g.second.physical_count==identity.groups.at(g.first).physical_count,"group physical count");near(g.second.mass_kg,identity.groups.at(g.first).mass_kg);near(g.second.mass_kg/inv.total_mass_kg,identity.groups.at(g.first).mass_kg/identity.total_mass_kg);}
  for(std::size_t i=0;i<bins.size();++i){const auto& b=inv.bins[i];check(b.physical.bin==bins[i].bin&&b.physical.group==bins[i].group&&b.physical.diameter_m==bins[i].diameter_m,"PSD membership");near(b.computational_particle_mass_kg,q*b.physical_particle_mass_kg);const auto p=sphere_properties(bins[i].diameter_m,{bins[i].density_kg_m3});near(b.computational_inertia_kg_m2,p.inertia_kg_m2*q*b.length_scale*b.length_scale);near(b.computational_particle_mass_kg*b.computational_count,b.total_mass_kg);
   if(q==1)check(b.computational_diameter_m==b.physical.diameter_m&&b.computational_particle_mass_kg==p.mass_kg&&b.computational_inertia_kg_m2==p.inertia_kg_m2,"identity bits");
  }
 }
 rejects([&]{representative_inventory(bins,0);});rejects([&]{representative_inventory(bins,7);});rejects([&]{representative_inventory({},1);});
 auto bad=bins;bad.push_back(bins[0]);rejects([&]{representative_inventory(bad,1);});bad=bins;bad[0].physical_count=0;rejects([&]{representative_inventory(bad,1);});bad=bins;bad[1].density_kg_m3=2500;rejects([&]{representative_inventory(bad,1);});
 for(double invalid:{0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){bad=bins;bad[0].diameter_m=invalid;rejects([&]{representative_inventory(bad,1);});bad=bins;bad[0].density_kg_m3=invalid;rejects([&]{representative_inventory(bad,1);});}
 bad=bins;bad[0].physical_count=MaximumExactRepresentativeCount;rejects([&]{representative_inventory(bad,1);});rejects([&]{representative_length_scale(std::numeric_limits<std::uint64_t>::max());});
 const auto physical=model();
 {auto extreme=physical;extreme.linear.kn=std::numeric_limits<double>::max();rejects([&]{representative_contact(extreme,8,RepresentativeScope::DryBulkCandidate);});}
 for(auto q:{2ull,8ull,27ull}){auto m=representative_contact(physical,q,RepresentativeScope::DryBulkCandidate);const auto s=m.length_scale;
  check(m.requires_bulk_validation&&m.physical.linear.kn==physical.linear.kn&&m.computational.linear.mu==physical.linear.mu,"material provenance");near(m.computational.linear.kn/s,physical.linear.kn);near(m.computational.linear.cn/(s*s),physical.linear.cn);
  Particle a,b;a.id=1;b.id=2;a.diameter_m=b.diameter_m=.002;a.material.density_kg_m3=b.material.density_kg_m3=1500;a.position_m={0,0,.0009999};b.position_m={.0019999,0,.0009999};a.velocity_m_s={.001,.002,0};a.angular_velocity_rad_s={.1,.2,.3};
  auto ca=scale(a,s),cb=scale(b,s);ContactHistory ph,ch,wh,cwh;
  for(int i=0;i<50;++i){auto p=evaluate_material_pair(a,b,physical,ph,1e-8),c=evaluate_material_pair(ca,cb,m.computational,ch,s*1e-8);vector_scaled(p.force_a,c.force_a,s*s);vector_scaled(p.torque_a,c.torque_a,s*s*s);vector_scaled(ph.displacement_m,ch.displacement_m,s);
   auto wp=evaluate_material_plane(a,{0,0,1},0,physical,wh,1e-8),wc=evaluate_material_plane(ca,{0,0,1},0,m.computational,cwh,s*1e-8);vector_scaled(wp.force_a,wc.force_a,s*s);vector_scaled(wp.torque_a,wc.torque_a,s*s*s);
  }
  const auto pm=sphere_properties(.002,{1500}),cm=sphere_properties(.002*s,{1500});near(physical.linear.cn/std::sqrt(physical.linear.kn*pm.mass_kg),m.computational.linear.cn/std::sqrt(m.computational.linear.kn*cm.mass_kg));
 }
 for(auto scope:{RepresentativeScope::ClearanceOrClogging,RepresentativeScope::SizeSegregation,RepresentativeScope::ImpactOrDeformation})rejects([&]{representative_contact(physical,8,scope);});
 rejects([&]{representative_contact(physical,1,static_cast<RepresentativeScope>(99));});rejects([&]{representative_contact(physical,1,RepresentativeScope::DryBulkCandidate,true);});
 for(int kind=0;kind<5;++kind){auto m=physical;if(kind==0)m.law=ContactLaw::HertzMindlin;if(kind==1)m.rolling.mu=.1;if(kind==2)m.adhesion_gamma_j_m2=.01;if(kind==3)m.linear.rolling.mu=.1;if(kind==4)m.linear.adhesion_gamma_j_m2=.01;rejects([&]{representative_contact(m,8,RepresentativeScope::DryBulkCandidate);});auto same=representative_contact(m,1,RepresentativeScope::DryBulkCandidate);check(!same.requires_bulk_validation,"identity validation flag");}
 // Ratio one uses the actual existing linear/Hertz, adhesion and rolling solver.
 for(bool hertz:{false,true}){auto a=clump_demo();a.bodies.resize(2);a.bodies[1]=make_clump(2,1,{{{},.04}},2500);a.bodies[0].position_m={0,0,.0199999};a.bodies[1].position_m={.0399999,0,.0199999};for(auto& pair:a.pair_models){pair.second.law=hertz?ContactLaw::HertzMindlin:ContactLaw::Linear;pair.second.rolling.mu=.1;pair.second.adhesion_gamma_j_m2=.001;}auto b=a;
  for(auto& m:b.pair_models)m.second=representative_contact(m.second,1,RepresentativeScope::ImpactOrDeformation).computational;
  for(auto& m:b.wall_models)m.second=representative_contact(m.second,1,RepresentativeScope::ClearanceOrClogging).computational;
  for(int i=0;i<100;++i){a.step(1e-8);b.step(1e-8);check(serialize_clump_state(a)==serialize_clump_state(b)&&a.histories()==b.histories(),"ratio one solver changed");}
 }
 if(argc==2){const std::filesystem::path out=argv[1];check(std::filesystem::create_directory(out),"output must be new");for(auto q:{1ull,2ull,8ull}){std::ofstream f(out/("inventory_q"+std::to_string(q)+".csv"));f<<representative_inventory_csv(representative_inventory(bins,q));check(bool(f),"CSV output");}}
 std::cout<<"PASS PSD/group mass and composition, exact integer weights, SI/inertia, no remainder loss, ratio-one actual solver, linear pair/wall scaling, unsupported scopes rejected\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
