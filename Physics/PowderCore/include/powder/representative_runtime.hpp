#pragma once
#include "powder/representative.hpp"
#include "powder/clump.hpp"
namespace powder {
struct RepresentativePlacement { ParticleId id=0; std::uint32_t group=0,bin=0; Vec3 position_m,velocity_m_s; };
// Recompute derived fields from physical inputs; no mutable inventory is trusted.
inline ClumpWorld representative_world(const std::vector<PhysicalSizeBin>& bins,std::uint64_t q,
 const std::vector<RepresentativePlacement>& placements,
 const std::map<std::pair<std::uint32_t,std::uint32_t>,MaterialContact>& pairs,
 const std::map<std::uint32_t,MaterialContact>& walls,
 RepresentativeScope scope=RepresentativeScope::DryBulkCandidate) {
 const auto inventory=representative_inventory(bins,q);
 if(placements.size()!=inventory.computational_count)throw std::invalid_argument("representative placement count mismatch");
 ClumpWorld world; std::map<std::pair<std::uint32_t,std::uint32_t>,std::uint64_t> used;std::set<ParticleId> ids;
 for(const auto& a:inventory.groups){
  const auto wall=walls.find(a.first);if(wall==walls.end())throw std::invalid_argument("missing wall material");
  world.wall_models[a.first]=representative_contact(wall->second,q,scope).computational;
  for(const auto& b:inventory.groups)if(a.first<=b.first){const auto key=std::make_pair(a.first,b.first);auto p=pairs.find(key);if(p==pairs.end())throw std::invalid_argument("missing pair material");world.pair_models[key]=representative_contact(p->second,q,scope).computational;}
 }
 for(const auto& p:placements){
  if(!ids.insert(p.id).second)throw std::invalid_argument("duplicate representative ID");
  for(double x:{p.position_m.x,p.position_m.y,p.position_m.z,p.velocity_m_s.x,p.velocity_m_s.y,p.velocity_m_s.z})if(!std::isfinite(x))throw std::invalid_argument("nonfinite placement");
  auto b=std::find_if(inventory.bins.begin(),inventory.bins.end(),[&](const auto& v){return v.physical.group==p.group&&v.physical.bin==p.bin;});
  if(b==inventory.bins.end()||++used[{p.group,p.bin}]>b->computational_count)throw std::invalid_argument("representative bin quota mismatch");
  auto body=make_clump(p.id,p.group,{{{},b->computational_diameter_m}},b->physical.density_kg_m3);body.position_m=p.position_m;body.velocity_m_s=p.velocity_m_s;world.bodies.push_back(body);
 }
 return world;
}
}
