#include "powder/world.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
namespace powder {
namespace {
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
double length(Vec3 a){return std::hypot(a.x,a.y,a.z);}
double distance(Vec3 a,Vec3 b){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);}
void finite(double x){if(!std::isfinite(x))throw std::invalid_argument("nonfinite world setting");}
double separation(const Particle& a,const Particle& b){return std::hypot(a.position_m.x-b.position_m.x,a.position_m.y-b.position_m.y,a.position_m.z-b.position_m.z);}
double overlap(const Particle& a,const Particle& b){return a.diameter_m/2+b.diameter_m/2-separation(a,b);}
using Cell=std::array<long long,3>;
long long coordinate(double x,double width){
 const double cell=std::floor(x/width);
 if(!std::isfinite(cell)||std::abs(cell)>1e12)throw std::invalid_argument("grid coordinate out of supported range");
 return static_cast<long long>(cell);
}
void geometric_guard(const std::vector<Particle>& p,const std::vector<FixedPlane>& walls,const std::vector<FixedBox>& boxes,const MaterialContact& model,const MaterialContact& wall_model){
 for(const auto& a:p)for(const auto& c:box_contacts(a,boxes))if(wall_model.law==ContactLaw::HertzMindlin&&c.overlap_m>wall_model.max_overlap_radius_ratio*a.diameter_m/2)throw std::invalid_argument("world Hertz box overlap cap exceeded");
 auto contacts=find_contacts(p);
 std::map<ParticleId,const Particle*> lookup;for(const auto& a:p)lookup.emplace(a.id,&a);
 for(const auto& key:contacts.contacts){const auto& a=*lookup.at(key.first);const auto& b=*lookup.at(key.second);
  const double d=separation(a,b),o=overlap(a,b),r=std::min(a.diameter_m,b.diameter_m)/2;
  if(d==0||o>=2*r)throw std::invalid_argument("invalid/deep particle overlap");
  if(model.law==ContactLaw::HertzMindlin&&o>model.max_overlap_radius_ratio*r)throw std::invalid_argument("world Hertz pair overlap cap exceeded");
 }
 for(const auto& a:p)for(const auto& wall:walls){
  const double d=dot(a.position_m,wall.normal)-wall.offset_m,r=a.diameter_m/2;
  if(d<0)throw std::invalid_argument("particle centre crossed wall");
  if(wall_model.law==ContactLaw::HertzMindlin&&r-d>wall_model.max_overlap_radius_ratio*r)throw std::invalid_argument("world Hertz wall overlap cap exceeded");
 }
}
}
SearchResult find_contacts(const std::vector<Particle>& particles,bool brute){
 validate_particles(particles);SearchResult result;if(particles.size()<2)return result;
 std::vector<const Particle*> sorted;double width=0;
 for(const auto& p:particles){sorted.push_back(&p);width=std::max(width,p.diameter_m);}
 std::sort(sorted.begin(),sorted.end(),[](auto a,auto b){return a->id<b->id;});
 auto test=[&](const Particle& a,const Particle& b){++result.distance_checks;if(overlap(a,b)>0)result.contacts.emplace_back(a.id,b.id);};
 if(brute){for(std::size_t i=0;i<sorted.size();++i)for(std::size_t j=i+1;j<sorted.size();++j)test(*sorted[i],*sorted[j]);}
 else{
  std::map<Cell,std::vector<const Particle*>> cells;
  for(auto p:sorted)cells[{coordinate(p->position_m.x,width),coordinate(p->position_m.y,width),coordinate(p->position_m.z,width)}].push_back(p);
  for(const auto& bucket:cells)for(auto a:bucket.second){
   const auto c=bucket.first;
   for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)for(int z=-1;z<=1;++z){
    auto it=cells.find({c[0]+x,c[1]+y,c[2]+z});if(it==cells.end())continue;
    for(auto b:it->second)if(a->id<b->id)test(*a,*b);
   }
  }
 }
 std::sort(result.contacts.begin(),result.contacts.end());return result;
}
StepReport ParticleSystem::step(double dt,bool brute){
 validate_boxes(boxes);
 finite(physical_time_s);finite(moving_wall_work_j);if(physical_time_s<0)throw std::invalid_argument("negative world clock");
 finite(dt);if(dt<=0)throw std::invalid_argument("world dt must be positive");const auto& wall_material=wall_model?*wall_model:model;validate_contact_model(model);validate_contact_model(wall_material);
 finite(gravity_m_s2.x);finite(gravity_m_s2.y);finite(gravity_m_s2.z);validate_particles(particles);
 auto ordered=particles;std::sort(ordered.begin(),ordered.end(),[](const Particle& a,const Particle& b){return a.id<b.id;});
 auto walls=planes;std::sort(walls.begin(),walls.end(),[](const FixedPlane& a,const FixedPlane& b){return a.id<b.id;});
 std::set<std::uint64_t> ids;for(const auto& wall:walls){finite(wall.offset_m);finite(length(wall.normal));if(std::abs(length(wall.normal)-1)>1e-12||!ids.insert(wall.id).second)throw std::invalid_argument("invalid/duplicate wall");}
 const auto found=find_contacts(ordered,brute);StepReport report;report.distance_checks=found.distance_checks;report.pair_contacts=found.contacts.size();
 std::map<ParticleId,std::size_t> index;std::vector<std::size_t> degree(ordered.size(),0);
 double min_mass=std::numeric_limits<double>::max(),max_radius=0,min_radius=std::numeric_limits<double>::max();
 for(std::size_t i=0;i<ordered.size();++i){index[ordered[i].id]=i;const auto s=sphere_properties(ordered[i].diameter_m,ordered[i].material);min_mass=std::min(min_mass,s.mass_kg);max_radius=std::max(max_radius,s.radius_m);min_radius=std::min(min_radius,s.radius_m);report.total_mass_kg+=s.mass_kg;}
 finite(report.total_mass_kg);
 for(const auto& key:found.contacts){++degree[index.at(key.first)];++degree[index.at(key.second)];}
 for(std::size_t i=0;i<ordered.size();++i)for(const auto& wall:walls)if(ordered[i].diameter_m/2>dot(ordered[i].position_m,wall.normal)-wall.offset_m)++degree[i];
 std::vector<std::vector<BoxContact>> box_hits;for(std::size_t i=0;i<ordered.size();++i){box_hits.push_back(box_contacts(ordered[i],boxes));degree[i]+=box_hits.back().size();}
 std::vector<std::vector<MeshQuery>> mesh_hits(ordered.size());
 for(std::size_t i=0;i<ordered.size();++i)for(const auto& mesh:meshes){auto hits=mesh.query(ordered[i],brute);degree[i]+=hits.contacts.size();report.distance_checks+=hits.triangle_checks;mesh_hits[i].push_back(std::move(hits));}
 std::vector<std::vector<std::vector<MovingMeshHit>>> moving_hits(ordered.size());report.moving_wall_loads.resize(moving_meshes.size());
 for(const auto& mesh:moving_meshes)mesh.validate_step(physical_time_s,dt,ordered.empty()?1:min_radius);
 for(std::size_t i=0;i<ordered.size();++i)for(const auto& mesh:moving_meshes){auto hits=mesh.query(ordered[i],physical_time_s);degree[i]+=hits.size();moving_hits[i].push_back(std::move(hits));}
 if(!ordered.empty()){
  const double count=static_cast<double>(std::max<std::size_t>(1,*std::max_element(degree.begin(),degree.end())));
  double kn=0,kt=0,cn=0,ct=0;
  for(const auto* material:std::array<const MaterialContact*,2>{&model,&wall_material}){
   double nk=material->linear.kn,tk=material->linear.kt,nd=material->linear.cn,td=material->linear.ct;
   if(material->law==ContactLaw::HertzMindlin){auto v=hertz_response(material->a,material->b,max_radius,max_radius*material->max_overlap_radius_ratio);nk=v.normal_derivative_n_m;tk=v.tangent_stiffness_n_m;nd=material->cn;td=material->ct;}
   if(material->adhesion_gamma_j_m2>0)nk+=4*3.14159265358979323846*material->adhesion_gamma_j_m2*max_radius/min_radius;
   kn=std::max(kn,nk);kt=std::max(kt,tk);cn=std::max(cn,nd);ct=std::max(ct,td);
  }
  double rolling_rate=0;
  for(const auto* material:std::array<const MaterialContact*,2>{&model,&wall_material})if(material->rolling.mu>0){
   const auto& rr=material->rolling;const double inertia=.4*min_mass*min_radius*min_radius/2;
   rolling_rate=std::max(rolling_rate,std::sqrt(count*rr.stiffness_n_m*max_radius*max_radius/inertia)+count*rr.damping_ns_m*max_radius*max_radius/inertia);
  }
  const double rate=std::max(rolling_rate,std::max(std::sqrt(count*kn/(min_mass/2))+count*cn/(min_mass/2),std::sqrt(count*kt/(min_mass/7))+count*ct/(min_mass/7)));
  finite(rate);if(dt>.02/rate)throw std::invalid_argument("dt exceeds multi-contact world limit");
 }
 geometric_guard(ordered,walls,boxes,model,wall_material);
 std::vector<ForceAccumulator> forces(ordered.size()),torques(ordered.size());
 std::map<PairKey,ContactHistory> new_pairs,new_walls,new_boxes;
 for(std::size_t i=0;i<ordered.size();++i)forces[i].add_gravity(sphere_properties(ordered[i].diameter_m,ordered[i].material).mass_kg,gravity_m_s2);
 for(const auto& key:found.contacts){
  const auto i=index.at(key.first),j=index.at(key.second);ContactHistory history;
  auto old=pairs_.find(key);if(old!=pairs_.end())history=old->second;
  const auto wrench=evaluate_material_pair(ordered[i],ordered[j],model,history,dt);
  forces[i].add(wrench.force_a);forces[j].add(wrench.force_b);torques[i].add(wrench.torque_a);torques[j].add(wrench.torque_b);
  if(history.active)new_pairs.emplace(key,history);
  report.max_overlap_m=std::max(report.max_overlap_m,overlap(ordered[i],ordered[j]));
 }
 for(std::size_t i=0;i<ordered.size();++i)for(const auto& wall:walls){
  const double o=ordered[i].diameter_m/2-(dot(ordered[i].position_m,wall.normal)-wall.offset_m);if(o<=0)continue;
  const PairKey key{ordered[i].id,wall.id};ContactHistory history;auto old=walls_.find(key);if(old!=walls_.end())history=old->second;
  const auto w=evaluate_material_plane(ordered[i],wall.normal,wall.offset_m,wall_material,history,dt);
  forces[i].add(w.force_a);torques[i].add(w.torque_a);if(history.active)new_walls.emplace(key,history);
  ++report.wall_contacts;report.max_overlap_m=std::max(report.max_overlap_m,o);
 }
 for(std::size_t i=0;i<ordered.size();++i)for(const auto& c:box_hits[i]){
  const PairKey key{ordered[i].id,c.surface_id};ContactHistory history;auto old=boxes_history_.find(key);if(old!=boxes_history_.end())history=old->second;
  const auto w=evaluate_material_plane(ordered[i],c.normal,dot(c.normal,c.point_m),wall_material,history,dt);
  forces[i].add(w.force_a);torques[i].add(w.torque_a);if(history.active)new_boxes.emplace(key,history);
  ++report.wall_contacts;report.max_overlap_m=std::max(report.max_overlap_m,c.overlap_m);
 }
 std::map<MeshHistoryKey,ContactHistory> new_meshes;
 for(std::size_t i=0;i<ordered.size();++i)for(std::size_t j=0;j<meshes.size();++j)for(const auto& c:mesh_hits[i][j].contacts){
  const MeshHistoryKey key{ordered[i].id,j,c.feature_id};auto found_history=mesh_history_.find(key);auto h=found_history==mesh_history_.end()?ContactHistory{}:found_history->second;
  auto w=evaluate_material_plane(ordered[i],c.normal,dot(c.normal,c.point_m),wall_material,h,dt);forces[i].add(w.force_a);torques[i].add(w.torque_a);if(h.active)new_meshes.emplace(key,h);++report.wall_contacts;report.max_overlap_m=std::max(report.max_overlap_m,c.overlap_m);
 }
 std::map<MeshHistoryKey,ContactHistory> next_moving;
 for(std::size_t i=0;i<ordered.size();++i)for(std::size_t j=0;j<moving_meshes.size();++j)for(const auto& c:moving_hits[i][j]){MeshHistoryKey key{ordered[i].id,j,c.geometry.feature_id};auto old=moving_history_.find(key);auto h=old==moving_history_.end()?ContactHistory{}:old->second;auto w=evaluate_moving_wall(ordered[i],c,wall_material,h,dt);forces[i].add(w.particle.force_a);torques[i].add(w.particle.torque_a);if(h.active)next_moving.emplace(key,h);auto& load=report.moving_wall_loads[j];for(auto member:{&Vec3::x,&Vec3::y,&Vec3::z}){load.reaction_force_n.*member+=w.wall.reaction_force_n.*member;load.reaction_torque_nm.*member+=w.wall.reaction_torque_nm.*member;}load.power_to_particles_w+=w.wall.power_to_particles_w;report.moving_wall_work_j+=w.wall.power_to_particles_w*dt;++report.wall_contacts;report.max_overlap_m=std::max(report.max_overlap_m,c.geometry.overlap_m);}
 auto next=ordered;
 for(std::size_t i=0;i<next.size();++i){integrate_rigid(next[i],forces[i],torques[i].total_n(),dt);
  const double travel=distance(next[i].position_m,ordered[i].position_m); // Discrete search, not CCD.
  if(travel>.1*min_radius)throw std::invalid_argument("step displacement too large for discrete contact search");
  report.max_speed_m_s=std::max(report.max_speed_m_s,length(next[i].velocity_m_s));
 }
 for(std::size_t i=0;i<next.size();++i)for(const auto& mesh:meshes){mesh.guard_motion(ordered[i].position_m,next[i].position_m);for(const auto& c:mesh.query(next[i]).contacts)if(wall_material.law==ContactLaw::HertzMindlin&&c.overlap_m>wall_material.max_overlap_radius_ratio*next[i].diameter_m/2)throw std::invalid_argument("world Hertz mesh overlap cap exceeded");}
 for(std::size_t i=0;i<next.size();++i)for(const auto& mesh:moving_meshes){mesh.guard_motion(ordered[i].position_m,next[i].position_m,physical_time_s,dt);for(const auto& c:mesh.query(next[i],physical_time_s+dt))if(wall_material.law==ContactLaw::HertzMindlin&&c.geometry.overlap_m>wall_material.max_overlap_radius_ratio*next[i].diameter_m/2)throw std::invalid_argument("moving mesh Hertz overlap cap");}
 finite(physical_time_s+dt);finite(moving_wall_work_j+report.moving_wall_work_j);
 geometric_guard(next,walls,boxes,model,wall_material);
 // Preserve caller array order, but sum and integrate deterministically in ID order.
 auto committed=particles;for(auto& p:committed)p=next[index.at(p.id)];
 particles.swap(committed);pairs_.swap(new_pairs);walls_.swap(new_walls);boxes_history_.swap(new_boxes);mesh_history_.swap(new_meshes);moving_history_.swap(next_moving);physical_time_s+=dt;moving_wall_work_j+=report.moving_wall_work_j;return report;
}
}
