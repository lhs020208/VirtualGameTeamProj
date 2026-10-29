#include "powder/clump.hpp"
#include "powder/spatial.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
namespace powder {namespace {
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 mul(Vec3 a,double s){return {a.x*s,a.y*s,a.z*s};}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double norm(Vec3 a){return std::hypot(a.x,a.y,a.z);}
void require(bool b,const char* s){if(!b)throw std::invalid_argument(s);}
void finite(double x){require(std::isfinite(x),"nonfinite clump value");}
void finite(Vec3 a){finite(a.x);finite(a.y);finite(a.z);}
Vec3 mv(Tensor3 a,Vec3 b){return {a.v[0]*b.x+a.v[1]*b.y+a.v[2]*b.z,a.v[3]*b.x+a.v[4]*b.y+a.v[5]*b.z,a.v[6]*b.x+a.v[7]*b.y+a.v[8]*b.z};}
Quaternion product(Quaternion a,Quaternion b){return {a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w};}
Quaternion normalize(Quaternion q){double l=std::hypot(std::hypot(q.w,q.x),std::hypot(q.y,q.z));finite(l);require(l>0,"zero clump quaternion");return {q.w/l,q.x/l,q.y/l,q.z/l};}
Vec3 rotate(Quaternion q,Vec3 v){auto a=product(product(q,{0,v.x,v.y,v.z}),{q.w,-q.x,-q.y,-q.z});return {a.x,a.y,a.z};}
Vec3 invrotate(Quaternion q,Vec3 v){return rotate({q.w,-q.x,-q.y,-q.z},v);}
Quaternion increment(Vec3 w,double dt){double length=norm(w);finite(length);if(length==0)return {};double a=length*dt;finite(a);double s=std::sin(a/2)/length;return {std::cos(a/2),w.x*s,w.y*s,w.z*s};}
Tensor3 inverse(Tensor3 a){const auto& x=a.v;double d=x[0]*(x[4]*x[8]-x[5]*x[7])-x[1]*(x[3]*x[8]-x[5]*x[6])+x[2]*(x[3]*x[7]-x[4]*x[6]);finite(d);require(x[0]>0&&x[0]*x[4]-x[1]*x[3]>0&&d>0,"nonpositive clump inertia");Tensor3 o{{(x[4]*x[8]-x[5]*x[7])/d,(x[2]*x[7]-x[1]*x[8])/d,(x[1]*x[5]-x[2]*x[4])/d,(x[5]*x[6]-x[3]*x[8])/d,(x[0]*x[8]-x[2]*x[6])/d,(x[2]*x[3]-x[0]*x[5])/d,(x[3]*x[7]-x[4]*x[6])/d,(x[1]*x[6]-x[0]*x[7])/d,(x[0]*x[4]-x[1]*x[3])/d}};for(double v:o.v)finite(v);return o;}
Vec3 omega(const ClumpProperties& p,Quaternion q,Vec3 L){return rotate(q,mv(p.inverse_inertia_body,invrotate(q,L)));}
void validate_dynamic(const ClumpBody& b){require(b.id>0&&b.id<=(std::numeric_limits<ParticleId>::max()-32)/64,"invalid clump id");finite(b.position_m);finite(b.velocity_m_s);finite(b.angular_momentum_world);auto q=normalize(b.orientation);require(std::abs(q.w-b.orientation.w)+std::abs(q.x-b.orientation.x)+std::abs(q.y-b.orientation.y)+std::abs(q.z-b.orientation.z)<1e-10,"clump orientation must be unit");}
ClumpProperties validate_body(const ClumpBody& b){validate_dynamic(b);auto p=clump_properties(b.pebbles,b.density_kg_m3);require(norm(p.centroid_m)<1e-10*std::max(1.,p.bounding_radius_m),"clump pebbles must be COM centred");return p;}
bool same_geometry(const std::vector<Pebble>& a,const std::vector<Pebble>& b){
 if(a.size()!=b.size())return false;
 for(std::size_t i=0;i<a.size();++i)if(a[i].diameter_m!=b[i].diameter_m||a[i].local_m.x!=b[i].local_m.x||a[i].local_m.y!=b[i].local_m.y||a[i].local_m.z!=b[i].local_m.z)return false;
 return true;
}
bool centred_sphere(const ClumpBody& b){return b.pebbles.size()==1&&b.pebbles[0].local_m.x==0&&b.pebbles[0].local_m.y==0&&b.pebbles[0].local_m.z==0;}
auto material_values(const MaterialContact& m){
 return std::make_tuple(m.law,m.linear.kn,m.linear.cn,m.linear.kt,m.linear.ct,m.linear.mu,
 m.linear.rolling.stiffness_n_m,m.linear.rolling.damping_ns_m,m.linear.rolling.mu,m.linear.adhesion_gamma_j_m2,
 m.a.young_pa,m.a.poisson,m.b.young_pa,m.b.poisson,m.cn,m.ct,m.mu,m.max_overlap_radius_ratio,
 m.adhesion_gamma_j_m2,m.rolling.stiffness_n_m,m.rolling.damping_ns_m,m.rolling.mu);
}
template<class Map>bool same_materials(const Map& a,const Map& b){
 if(a.size()!=b.size())return false;
 auto j=b.begin();for(const auto& item:a){if(item.first!=j->first||material_values(item.second)!=material_values(j->second))return false;++j;}return true;
}
void append_prepared_particles(const ClumpBody& b,const ClumpProperties& props,std::vector<Particle>& out){
 const bool sphere=centred_sphere(b);
 const auto w=sphere?mul(b.angular_momentum_world,props.inverse_inertia_body.v[0]):omega(props,b.orientation,b.angular_momentum_world);

 for(std::size_t i=0;i<b.pebbles.size();++i){const auto& n=b.pebbles[i];Particle p;p.id=b.id*64+i;p.diameter_m=n.diameter_m;p.material.density_kg_m3=b.density_kg_m3;
 if(sphere){p.position_m=b.position_m;p.velocity_m_s=b.velocity_m_s;}
 else{auto arm=rotate(b.orientation,n.local_m);p.position_m=add(b.position_m,arm);p.velocity_m_s=add(b.velocity_m_s,cross(w,arm));}
 p.angular_velocity_rad_s=w;p.orientation=b.orientation;validate(p);out.push_back(p);}
}

}
ClumpProperties clump_properties(const std::vector<Pebble>& nodes,double density){require(!nodes.empty()&&nodes.size()<=32,"clump requires 1..32 pebbles");ClumpProperties p;
 for(std::size_t i=0;i<nodes.size();++i){auto& n=nodes[i];finite(n.local_m);auto s=sphere_properties(n.diameter_m,{density});p.mass_kg+=s.mass_kg;p.centroid_m=add(p.centroid_m,mul(n.local_m,s.mass_kg));for(std::size_t j=0;j<i;++j)require(norm(add(n.local_m,mul(nodes[j].local_m,-1)))+1e-12*std::max(n.diameter_m,nodes[j].diameter_m)>=(n.diameter_m+nodes[j].diameter_m)/2,"overlapping clump pebbles unsupported: mass would be double counted");}
 finite(p.mass_kg);p.centroid_m=mul(p.centroid_m,1/p.mass_kg);finite(p.centroid_m);
 for(auto& n:nodes){auto s=sphere_properties(n.diameter_m,{density});auto d=add(n.local_m,mul(p.centroid_m,-1));double x[3]={d.x,d.y,d.z};for(int i=0;i<3;++i)for(int j=0;j<3;++j)p.inertia_body.v[i*3+j]+=(i==j?s.inertia_kg_m2+s.mass_kg*dot(d,d):0)-s.mass_kg*x[i]*x[j];p.bounding_radius_m=std::max(p.bounding_radius_m,norm(d)+s.radius_m);}
 for(double v:p.inertia_body.v)finite(v);p.inverse_inertia_body=inverse(p.inertia_body);return p;
}
ClumpBody make_clump(ParticleId id,std::uint32_t type,const std::vector<Pebble>& nodes,double density){auto p=clump_properties(nodes,density);ClumpBody b;b.id=id;b.type=type;b.density_kg_m3=density;b.pebbles=nodes;for(auto& n:b.pebbles)n.local_m=add(n.local_m,mul(p.centroid_m,-1));validate_body(b);return b;}
Vec3 clump_omega(const ClumpBody& b){return omega(validate_body(b),b.orientation,b.angular_momentum_world);}
std::vector<Particle> clump_particles(const ClumpBody& b){
 if(centred_sphere(b)){validate_dynamic(b);const auto props=sphere_properties(b.pebbles[0].diameter_m,{b.density_kg_m3});Particle p;p.id=b.id*64;p.diameter_m=b.pebbles[0].diameter_m;p.material.density_kg_m3=b.density_kg_m3;p.position_m=b.position_m;p.velocity_m_s=b.velocity_m_s;p.angular_velocity_rad_s=mul(b.angular_momentum_world,1/props.inertia_kg_m2);p.orientation=b.orientation;validate(p);return {p};}
 auto w=clump_omega(b);std::vector<Particle> out;for(std::size_t i=0;i<b.pebbles.size();++i){auto& n=b.pebbles[i];auto arm=rotate(b.orientation,n.local_m);Particle p;p.id=b.id*64+i;p.diameter_m=n.diameter_m;p.material.density_kg_m3=b.density_kg_m3;p.position_m=add(b.position_m,arm);p.velocity_m_s=add(b.velocity_m_s,cross(w,arm));p.angular_velocity_rad_s=w;p.orientation=b.orientation;validate(p);out.push_back(p);}return out;}
static void integrate_prepared(ClumpBody& b,Vec3 force,Vec3 torque,double dt,const ClumpProperties& p,bool sphere,bool cached){finite(force);finite(torque);finite(dt);require(dt>0,"invalid clump dt");auto next=b;next.velocity_m_s=add(b.velocity_m_s,mul(force,dt/p.mass_kg));next.position_m=add(b.position_m,mul(next.velocity_m_s,dt));next.angular_momentum_world=add(b.angular_momentum_world,mul(torque,dt));Vec3 midL=mul(add(b.angular_momentum_world,next.angular_momentum_world),.5),w=sphere?mul(midL,p.inverse_inertia_body.v[0]):omega(p,b.orientation,midL);bool converged=sphere;
 if(sphere)require(norm(w)*dt<=.05,"clump angular step too large");
 for(int i=0;!sphere&&i<40;++i){require(norm(w)*dt<=.05,"clump angular step too large");auto half=normalize(product(increment(w,dt*.5),b.orientation));auto nw=omega(p,half,midL);if(norm(add(nw,mul(w,-1)))<1e-12*(1+norm(nw))){w=nw;converged=true;break;}w=nw;}
 require(converged,"clump midpoint orientation failed to converge");next.orientation=normalize(product(increment(w,dt),b.orientation));if(cached)validate_dynamic(next);else validate_body(next);b=std::move(next);
}
void integrate_clump(ClumpBody& b,Vec3 force,Vec3 torque,double dt){integrate_prepared(b,force,torque,dt,validate_body(b),false,false);}
double clump_kinetic_energy(const ClumpBody& b){auto p=validate_body(b);double e=.5*p.mass_kg*dot(b.velocity_m_s,b.velocity_m_s)+.5*dot(b.angular_momentum_world,omega(p,b.orientation,b.angular_momentum_world));finite(e);return e;}
ClumpReport ClumpWorld::step(double dt){return step_profiled(dt,nullptr);}
ClumpReport ClumpWorld::step_profiled(double dt,ClumpProfile* profile){ClumpProfileScope timer(profile);ParallelExecutor::validate(cpu_threads,parallel_min_jobs);finite(physical_time_s);finite(moving_wall_work_j);require(physical_time_s>=0,"negative clump clock");finite(dt);require(dt>0,"invalid clump dt");finite(gravity_m_s2);validate_boxes(boxes);require(maximum_bodies>=1&&maximum_bodies<=2000000&&maximum_pebbles>=1&&maximum_pebbles<=2000000&&maximum_candidates>=1&&maximum_candidates<=64000000,"invalid clump capacity contract");require(bodies.size()<=maximum_bodies,maximum_bodies==300?"clump body limit 300":"clump body capacity exceeded");std::size_t pebble_count=0;for(const auto& body:bodies){require(body.pebbles.size()<=maximum_pebbles-pebble_count,"clump pebble capacity exceeded");pebble_count+=body.pebbles.size();}auto& ordered=ordered_;ordered=bodies;std::sort(ordered.begin(),ordered.end(),[](auto& a,auto& b){return a.id<b.id;});std::set<ParticleId> ids;std::set<std::uint64_t> plane_ids;for(auto& p:planes){finite(p.normal);finite(p.offset_m);require(std::abs(norm(p.normal)-1)<1e-12&&plane_ids.insert(p.id).second,"invalid clump plane");}
 // Public configuration can be edited directly. Exact field comparisons detect edits;
 // snapshots are published only after the entire material table has validated.
 std::vector<std::uint32_t> types;for(const auto& b:ordered)types.push_back(b.type);
 std::sort(types.begin(),types.end());types.erase(std::unique(types.begin(),types.end()),types.end());
 if(use_cached_sphere_path){
  if(!material_cache_valid_||types!=cached_types_||!same_materials(pair_models,cached_pairs_)||!same_materials(wall_models,cached_walls_)){
   for(auto type:types){require(wall_models.count(type)!=0,"missing clump wall material");validate_contact_model(wall_models.at(type));
    for(auto other:types){auto key=std::minmax(type,other);require(pair_models.count(key)!=0,"missing clump pair material");validate_contact_model(pair_models.at(key));if(profile)++profile->material_pair_validations;}}
   cached_types_=types;cached_pairs_=pair_models;cached_walls_=wall_models;material_cache_valid_=true;if(profile)++profile->material_table_rebuilds;
  }
  std::set<ParticleId> current;for(const auto& b:ordered)current.insert(b.id);
  for(auto it=body_cache_.begin();it!=body_cache_.end();)if(!current.count(it->first))it=body_cache_.erase(it);else ++it;
 }
 auto& nodes=nodes_;nodes.clear();auto& owner=owner_;owner.clear();auto& props=props_;props.clear();ClumpReport report;double min_radius=std::numeric_limits<double>::max(),mobility=0;
 for(std::size_t i=0;i<ordered.size();++i){auto& b=ordered[i];require(ids.insert(b.id).second,"duplicate clump id");ClumpProperties p;
  if(use_cached_sphere_path){validate_dynamic(b);auto it=body_cache_.find(b.id);
   if(it==body_cache_.end()||it->second.density!=b.density_kg_m3||!same_geometry(it->second.pebbles,b.pebbles)){
    p=validate_body(b);body_cache_[b.id]={b.density_kg_m3,b.pebbles,p};if(profile)++profile->property_rebuilds;
   }else{p=it->second.properties;if(profile)++profile->property_hits;}
  }else p=validate_body(b);
  props.push_back(p);report.mass_kg+=p.mass_kg;double invbound=0;for(int row=0;row<3;++row){double sum=0;for(int col=0;col<3;++col)sum+=std::abs(p.inverse_inertia_body.v[row*3+col]);invbound=std::max(invbound,sum);}mobility=std::max(mobility,1/p.mass_kg+p.bounding_radius_m*p.bounding_radius_m*invbound);
  const auto start=nodes.size();if(use_cached_sphere_path)append_prepared_particles(b,p,nodes);else{auto ns=clump_particles(b);nodes.insert(nodes.end(),ns.begin(),ns.end());}for(std::size_t n=start;n<nodes.size();++n){owner.push_back(i);min_radius=std::min(min_radius,nodes[n].diameter_m/2);}if(!use_cached_sphere_path){require(wall_models.count(b.type)!=0,"missing clump wall material");validate_contact_model(wall_models.at(b.type));for(auto& other:ordered){if(profile)++profile->material_pair_validations;auto key=std::minmax(b.type,other.type);require(pair_models.count(key)!=0,"missing clump pair material");validate_contact_model(pair_models.at(key));}}}
 require(nodes.size()<=maximum_pebbles,"clump pebble capacity exceeded");finite(report.mass_kg);
 auto& loads=loads_;auto& torques=torques_;loads.assign(ordered.size(),{});torques.assign(ordered.size(),{});for(std::size_t i=0;i<ordered.size();++i)loads[i].add_gravity(props[i].mass_kg,gravity_m_s2);
 for(const auto& entry:external_wrenches){auto it=std::lower_bound(ordered.begin(),ordered.end(),entry.first,[](const auto& b,ParticleId id){return b.id<id;});require(it!=ordered.end()&&it->id==entry.first,"external wrench body missing");finite(entry.second.force_n);finite(entry.second.torque_nm);const auto i=static_cast<std::size_t>(it-ordered.begin());loads[i].add(entry.second.force_n);torques[i].add(entry.second.torque_nm);}
 auto& next_pairs=scratch_pairs_;auto& next_walls=scratch_walls_;auto& next_boxes=scratch_boxes_;next_pairs.clear();next_walls.clear();next_boxes.clear();auto& degrees=degrees_;degrees.assign(ordered.size(),0);double max_stiffness=0,max_damping=0;
 auto bound=[&](const MaterialContact& m,double radius){double k=std::max(m.linear.kn,m.linear.kt),c=std::max(m.linear.cn,m.linear.ct);if(m.law==ContactLaw::HertzMindlin){auto h=hertz_response(m.a,m.b,radius,radius*m.max_overlap_radius_ratio);k=std::max(h.normal_derivative_n_m,h.tangent_stiffness_n_m);c=std::max(m.cn,m.ct);}k+=4*3.14159265358979323846*m.adhesion_gamma_j_m2*radius/min_radius;if(m.rolling.mu>0){k=std::max(k,m.rolling.stiffness_n_m);c=std::max(c,m.rolling.damping_ns_m);}max_stiffness=std::max(max_stiffness,k);max_damping=std::max(max_damping,c);};
 auto apply=[&](std::size_t node,Vec3 f,Vec3 t){auto i=owner[node];loads[i].add(f);if(use_cached_sphere_path&&centred_sphere(ordered[i]))torques[i].add(t);else torques[i].add(add(t,cross(add(nodes[node].position_m,mul(ordered[i].position_m,-1)),f)));};
 timer.phase(1);
 const auto& candidates=neighbors_.query(nodes,owner,use_spatial_search,reuse_neighbor_list,profile,maximum_candidates);
 if(ParallelExecutor::lanes(candidates.size(),cpu_threads,parallel_min_jobs)==1){
 for(const auto& candidate:candidates){const auto i=candidate.first,j=candidate.second;if(profile)++profile->pair_distance_checks;double overlap=(nodes[i].diameter_m+nodes[j].diameter_m)/2-norm(add(nodes[i].position_m,mul(nodes[j].position_m,-1)));if(overlap<=0)continue;auto key=std::minmax(ordered[owner[i]].type,ordered[owner[j]].type);const auto& m=pair_models.at(key);bound(m,std::max(nodes[i].diameter_m,nodes[j].diameter_m)/2);++degrees[owner[i]];++degrees[owner[j]];PairKey contact{nodes[i].id,nodes[j].id};auto h=pairs_.count(contact)?pairs_.at(contact):ContactHistory{};auto v=evaluate_material_pair(nodes[i],nodes[j],m,h,dt);apply(i,v.force_a,v.torque_a);apply(j,v.force_b,v.torque_b);if(h.active)next_pairs.emplace(contact,h);++report.pair_contacts;report.max_overlap_m=std::max(report.max_overlap_m,overlap);}
 }else{
  pair_results_.resize(candidates.size());
  // Each candidate owns one output/history slot. Old histories/materials are read-only.
  report.pair_lanes=executor_.run(candidates.size(),cpu_threads,parallel_min_jobs,[&](std::size_t k){
   const auto i=candidates[k].first,j=candidates[k].second;auto& result=pair_results_[k];
   result.overlap=(nodes[i].diameter_m+nodes[j].diameter_m)/2-norm(add(nodes[i].position_m,mul(nodes[j].position_m,-1)));
   if(result.overlap<=0)return;
   const auto& material=pair_models.at(std::minmax(ordered[owner[i]].type,ordered[owner[j]].type));
   PairKey key{nodes[i].id,nodes[j].id};result.history=pairs_.count(key)?pairs_.at(key):ContactHistory{};
   result.wrench=evaluate_material_pair(nodes[i],nodes[j],material,result.history,dt);
  });
  // Deterministic reduction preserves the serial floating-point addition order.
  if(profile)profile->pair_distance_checks+=candidates.size();
  for(std::size_t k=0;k<candidates.size();++k){const auto& result=pair_results_[k];if(result.overlap<=0)continue;
   const auto i=candidates[k].first,j=candidates[k].second;
   const auto& material=pair_models.at(std::minmax(ordered[owner[i]].type,ordered[owner[j]].type));
   bound(material,std::max(nodes[i].diameter_m,nodes[j].diameter_m)/2);++degrees[owner[i]];++degrees[owner[j]];
   apply(i,result.wrench.force_a,result.wrench.torque_a);apply(j,result.wrench.force_b,result.wrench.torque_b);
   if(result.history.active)next_pairs.emplace(PairKey{nodes[i].id,nodes[j].id},result.history);
   ++report.pair_contacts;report.max_overlap_m=std::max(report.max_overlap_m,result.overlap);
  }
 }
 timer.phase(2);
 for(std::size_t i=0;i<nodes.size();++i){const auto& m=wall_models.at(ordered[owner[i]].type);bound(m,nodes[i].diameter_m/2);auto hit=[&](Vec3 n,double offset,std::uint64_t id,double overlap,bool box){if(overlap<=0)return;++degrees[owner[i]];PairKey key{nodes[i].id,id};const auto& old=box?box_history_:walls_;auto h=old.count(key)?old.at(key):ContactHistory{};auto v=evaluate_material_plane(nodes[i],n,offset,m,h,dt);apply(i,v.force_a,v.torque_a);if(h.active)(box?next_boxes:next_walls).emplace(key,h);++report.wall_contacts;report.max_overlap_m=std::max(report.max_overlap_m,overlap);};for(auto& plane:planes){double d=dot(nodes[i].position_m,plane.normal)-plane.offset_m;require(d>=0,"clump centre crossed plane");hit(plane.normal,plane.offset_m,plane.id,nodes[i].diameter_m/2-d,false);}for(auto& c:box_contacts(nodes[i],boxes))hit(c.normal,dot(c.normal,c.point_m),c.surface_id,c.overlap_m,true);}
 auto record_wall=[&](const MeshQuery& q){if(profile){profile->wall_triangle_checks+=q.triangle_checks;profile->wall_analytic_queries+=q.analytic_queries;profile->wall_bounds_rejects+=q.bounds_rejects;}};
 auto static_query=[&](const StaticTriangleMesh& mesh,const Particle& p){auto q=use_prepared_walls?mesh.query_optimized(p):mesh.query(p);record_wall(q);return q;};
 timer.phase(3);
 auto& next_meshes=scratch_meshes_;next_meshes.clear();
 for(std::size_t i=0;i<nodes.size();++i)for(std::size_t j=0;j<meshes.size();++j){const auto& material=wall_models.at(ordered[owner[i]].type);for(const auto& c:static_query(meshes[j],nodes[i]).contacts){++degrees[owner[i]];MeshHistoryKey key{nodes[i].id,j,c.feature_id};auto old=mesh_history_.find(key);auto h=old==mesh_history_.end()?ContactHistory{}:old->second;auto w=evaluate_material_plane(nodes[i],c.normal,dot(c.normal,c.point_m),material,h,dt);apply(i,w.force_a,w.torque_a);if(h.active)next_meshes.emplace(key,h);++report.wall_contacts;report.max_overlap_m=std::max(report.max_overlap_m,c.overlap_m);}}
 timer.phase(4);
 auto& next_moving=scratch_moving_;next_moving.clear();report.moving_wall_loads.resize(moving_meshes.size());
 for(const auto& mesh:moving_meshes)mesh.validate_step(physical_time_s,dt,nodes.empty()?1:min_radius);
 wall_poses_.clear();if(use_prepared_walls)for(const auto& mesh:moving_meshes){wall_poses_.emplace_back(mesh.motion(),physical_time_s);wall_poses_.emplace_back(mesh.motion(),physical_time_s+dt);if(profile)profile->wall_pose_builds+=2;}
 auto moving_query=[&](std::size_t j,const Particle& p,bool future){MeshQuery q;auto hits=use_prepared_walls?moving_meshes[j].query_prepared(p,wall_poses_[2*j+(future?1:0)],&q):moving_meshes[j].query(p,physical_time_s+(future?dt:0),&q);record_wall(q);return hits;};
 for(std::size_t i=0;i<nodes.size();++i)for(std::size_t j=0;j<moving_meshes.size();++j)for(const auto& c:moving_query(j,nodes[i],false)){const auto& material=wall_models.at(ordered[owner[i]].type);++degrees[owner[i]];MeshHistoryKey key{nodes[i].id,j,c.geometry.feature_id};auto old=moving_history_.find(key);auto h=old==moving_history_.end()?ContactHistory{}:old->second;auto w=evaluate_moving_wall(nodes[i],c,material,h,dt);apply(i,w.particle.force_a,w.particle.torque_a);if(h.active)next_moving.emplace(key,h);auto& load=report.moving_wall_loads[j];load.reaction_force_n=add(load.reaction_force_n,w.wall.reaction_force_n);load.reaction_torque_nm=add(load.reaction_torque_nm,w.wall.reaction_torque_nm);load.power_to_particles_w+=w.wall.power_to_particles_w;report.moving_wall_work_j+=w.wall.power_to_particles_w*dt;++report.wall_contacts;report.max_overlap_m=std::max(report.max_overlap_m,c.geometry.overlap_m);}
 timer.phase(5);
 if(!ordered.empty()){
  report.step_scale=rigid_step_scale(mobility,max_stiffness,max_damping,*std::max_element(degrees.begin(),degrees.end()));
  report.step_scale.minimum_radius_m=min_radius;report.step_scale.minimum_body_mass_kg=props[0].mass_kg;
  for(const auto& p:props)report.step_scale.minimum_body_mass_kg=std::min(report.step_scale.minimum_body_mass_kg,p.mass_kg);
  for(const auto& mesh:moving_meshes)report.step_scale.moving_wall_limit_s=std::min(report.step_scale.moving_wall_limit_s,mesh.step_limit(min_radius));
  require(dt<=report.step_scale.contact_limit_s,"dt exceeds rigid clump multi-contact limit");
 }
 timer.phase(6);
 auto& next=next_;next=ordered;report.integration_lanes=executor_.run(next.size(),cpu_threads,parallel_min_jobs,[&](std::size_t i){
  const bool sphere=use_cached_sphere_path&&centred_sphere(next[i]);
  if(use_cached_sphere_path)integrate_prepared(next[i],loads[i].total_n(),torques[i].total_n(),dt,props[i],sphere,true);
  else integrate_clump(next[i],loads[i].total_n(),torques[i].total_n(),dt);
 });
 if(profile)for(const auto& body:next){if(use_cached_sphere_path&&centred_sphere(body))++profile->sphere_integrations;else ++profile->generic_integrations;}
 auto& future=future_;future.clear();for(std::size_t i=0;i<next.size();++i){if(use_cached_sphere_path)append_prepared_particles(next[i],props[i],future);else{auto ns=clump_particles(next[i]);future.insert(future.end(),ns.begin(),ns.end());}}
 timer.phase(7);
 for(std::size_t i=0;i<future.size();++i){require(norm(add(future[i].position_m,mul(nodes[i].position_m,-1)))<=.1*min_radius,"clump pebble displacement too large");auto& m=wall_models.at(next[owner[i]].type);for(auto& p:planes){double d=dot(future[i].position_m,p.normal)-p.offset_m;require(d>=0,"clump crossed plane");if(m.law==ContactLaw::HertzMindlin)require(future[i].diameter_m/2-d<=m.max_overlap_radius_ratio*future[i].diameter_m/2,"clump Hertz wall overlap cap");}for(auto& c:box_contacts(future[i],boxes))if(m.law==ContactLaw::HertzMindlin)require(c.overlap_m<=m.max_overlap_radius_ratio*future[i].diameter_m/2,"clump Hertz box overlap cap");}
 for(const auto& candidate:neighbors_.query(future,owner,use_spatial_search,reuse_neighbor_list,profile,maximum_candidates)){const auto i=candidate.second,j=candidate.first;if(profile)++profile->post_pair_checks;double overlap=(future[i].diameter_m+future[j].diameter_m)/2-norm(add(future[i].position_m,mul(future[j].position_m,-1)));double radius=std::min(future[i].diameter_m,future[j].diameter_m)/2;require(overlap<2*radius,"deep external clump overlap");auto key=std::minmax(next[owner[i]].type,next[owner[j]].type);auto& pair=pair_models.at(key);if(pair.law==ContactLaw::HertzMindlin)require(overlap<=pair.max_overlap_radius_ratio*radius,"clump Hertz pair overlap cap");}
 for(std::size_t i=0;i<future.size();++i)for(const auto& mesh:meshes){mesh.guard_motion(nodes[i].position_m,future[i].position_m);const auto& material=wall_models.at(next[owner[i]].type);for(const auto& c:static_query(mesh,future[i]).contacts)if(material.law==ContactLaw::HertzMindlin)require(c.overlap_m<=material.max_overlap_radius_ratio*future[i].diameter_m/2,"clump Hertz mesh overlap cap");}
 for(std::size_t i=0;i<future.size();++i)for(std::size_t j=0;j<moving_meshes.size();++j){const auto& mesh=moving_meshes[j];if(use_prepared_walls)mesh.guard_prepared(nodes[i].position_m,future[i].position_m,wall_poses_[2*j],wall_poses_[2*j+1]);else mesh.guard_motion(nodes[i].position_m,future[i].position_m,physical_time_s,dt);const auto& material=wall_models.at(next[owner[i]].type);for(const auto& c:moving_query(j,future[i],true))if(material.law==ContactLaw::HertzMindlin)require(c.geometry.overlap_m<=material.max_overlap_radius_ratio*future[i].diameter_m/2,"clump moving mesh Hertz overlap cap");}
 finite(physical_time_s+dt);finite(moving_wall_work_j+report.moving_wall_work_j);
 timer.phase(8);
 std::map<ParticleId,std::size_t> next_index;for(std::size_t i=0;i<next.size();++i)next_index.emplace(next[i].id,i);
 auto& committed=committed_;committed=bodies;for(auto& b:committed)b=next[next_index.at(b.id)];bodies.swap(committed);pairs_.swap(next_pairs);walls_.swap(next_walls);box_history_.swap(next_boxes);mesh_history_.swap(next_meshes);moving_history_.swap(next_moving);physical_time_s+=dt;moving_wall_work_j+=report.moving_wall_work_j;if(profile)profile->workspace_capacity_bytes=std::max<std::uint64_t>(profile->workspace_capacity_bytes,workspace_capacity_bytes());external_wrenches.clear();timer.accepted();return report;
}
std::size_t ClumpWorld::workspace_capacity_bytes()const{
 std::size_t bytes=pair_results_.capacity()*sizeof(PairResult)+wall_poses_.capacity()*sizeof(PreparedMotion)+(nodes_.capacity()+future_.capacity())*sizeof(Particle)+(owner_.capacity()+degrees_.capacity())*sizeof(std::size_t)+props_.capacity()*sizeof(ClumpProperties)+(loads_.capacity()+torques_.capacity())*sizeof(ForceAccumulator);
 for(const auto* v:{&ordered_,&next_,&committed_}){bytes+=v->capacity()*sizeof(ClumpBody);for(const auto& b:*v)bytes+=b.pebbles.capacity()*sizeof(Pebble);}
 bytes+=pairs_.capacity_bytes()+walls_.capacity_bytes()+box_history_.capacity_bytes()+mesh_history_.capacity_bytes()+moving_history_.capacity_bytes()+scratch_pairs_.capacity_bytes()+scratch_walls_.capacity_bytes()+scratch_boxes_.capacity_bytes()+scratch_meshes_.capacity_bytes()+scratch_moving_.capacity_bytes();return bytes;
}
ClumpWorld clump_demo(double powder_density,double inclusion_density){ClumpWorld w;MaterialContact soft;soft.law=ContactLaw::Linear;soft.linear={10000,8,2500,2,.4};auto hard=soft;hard.linear={20000,12,5000,3,.5};auto mixed=soft;mixed.linear={14000,10,3500,2.5,.45};w.pair_models={{{0,0},soft},{{0,1},mixed},{{1,1},hard}};w.wall_models={{0,soft},{1,hard}};FixedBox floor;floor.id=1;floor.centre_m={0,0,-.05};floor.half_m={.5,.4,.05};w.boxes={floor};
 for(int i=0;i<6;++i){bool inclusion=i%2!=0;auto b=make_clump(i+1,inclusion?1:0,inclusion?std::vector<Pebble>{{{-.02,0,0},.04},{{.02,0,0},.04}}:std::vector<Pebble>{{{},.04}},inclusion?inclusion_density:powder_density);b.position_m={-.16+.16*(i%3),-.08+.16*(i/3),.12+.03*(i%3)};if(inclusion){double a=.35;b.orientation={std::cos(a/2),0,std::sin(a/2),0};}w.bodies.push_back(b);}return w;
}
std::string serialize_clump_state(const ClumpWorld& w){std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17)<<"body_id,type,density_kg_m3,mass_kg,pebbles,x_m,y_m,z_m,vx_m_s,vy_m_s,vz_m_s,qw,qx,qy,qz,Lx,Ly,Lz,Ixx,Ixy,Ixz,Iyy,Iyz,Izz\n";for(auto& b:w.bodies){auto p=validate_body(b);auto& v=p.inertia_body.v;o<<b.id<<','<<b.type<<','<<b.density_kg_m3<<','<<p.mass_kg<<','<<b.pebbles.size()<<','<<b.position_m.x<<','<<b.position_m.y<<','<<b.position_m.z<<','<<b.velocity_m_s.x<<','<<b.velocity_m_s.y<<','<<b.velocity_m_s.z<<','<<b.orientation.w<<','<<b.orientation.x<<','<<b.orientation.y<<','<<b.orientation.z<<','<<b.angular_momentum_world.x<<','<<b.angular_momentum_world.y<<','<<b.angular_momentum_world.z<<','<<v[0]<<','<<v[1]<<','<<v[2]<<','<<v[4]<<','<<v[5]<<','<<v[8]<<'\n';}return o.str();}
}
