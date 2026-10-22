#pragma once
#include "powder/elastic.hpp"
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <locale>
namespace powder {
// Opt11 contract/data only. No runtime substitution, placement, or mixing index.
enum class RepresentativeScope { DryBulkCandidate, ClearanceOrClogging, SizeSegregation, ImpactOrDeformation };
struct PhysicalSizeBin {
 std::uint32_t group=0,bin=0;
 double diameter_m=0,density_kg_m3=0;
 std::uint64_t physical_count=0;
};
struct RepresentativeBin {
 PhysicalSizeBin physical;
 std::uint64_t represented_count_per_particle=1,computational_count=0;
 double length_scale=1,computational_diameter_m=0;
 double physical_particle_mass_kg=0,computational_particle_mass_kg=0;
 double computational_inertia_kg_m2=0,total_mass_kg=0;
};
struct GroupInventory {std::uint64_t physical_count=0,computational_count=0;double mass_kg=0;};
struct RepresentativeInventory {
 std::uint64_t represented_count_per_particle=1,physical_count=0,computational_count=0;
 double total_mass_kg=0;
 std::vector<RepresentativeBin> bins;
 std::map<std::uint32_t,GroupInventory> groups;
};
inline constexpr std::uint64_t MaximumExactRepresentativeCount=9007199254740991ull;
inline double representative_length_scale(std::uint64_t q){
 if(q<1||q>MaximumExactRepresentativeCount)throw std::invalid_argument("representative count must be an exact positive integer <= 2^53-1");
 return q==1?1:std::cbrt(static_cast<double>(q));
}
inline RepresentativeInventory representative_inventory(const std::vector<PhysicalSizeBin>& bins,std::uint64_t q){
 const double length=representative_length_scale(q);if(bins.empty())throw std::invalid_argument("empty physical PSD");
 RepresentativeInventory result;result.represented_count_per_particle=q;
 std::set<std::pair<std::uint32_t,std::uint32_t>> keys;std::map<std::uint32_t,double> density;
 for(const auto& bin:bins){
  if(!keys.emplace(bin.group,bin.bin).second)throw std::invalid_argument("duplicate group/bin key");
  if(bin.physical_count<1||bin.physical_count>MaximumExactRepresentativeCount-result.physical_count)throw std::invalid_argument("physical count outside exact integer range");
  if(bin.physical_count%q)throw std::invalid_argument("physical bin count not divisible by fixed representative count; no implicit rounding or mixed-scale tail");
  const auto physical=sphere_properties(bin.diameter_m,{bin.density_kg_m3});
  auto it=density.emplace(bin.group,bin.density_kg_m3);if(!it.second&&it.first->second!=bin.density_kg_m3)throw std::invalid_argument("one material density per group required");
  RepresentativeBin entry;entry.physical=bin;entry.represented_count_per_particle=q;entry.computational_count=bin.physical_count/q;entry.length_scale=length;
  entry.computational_diameter_m=q==1?bin.diameter_m:bin.diameter_m*length;
  const auto computational=sphere_properties(entry.computational_diameter_m,{bin.density_kg_m3});
  entry.physical_particle_mass_kg=physical.mass_kg;entry.computational_particle_mass_kg=computational.mass_kg;entry.computational_inertia_kg_m2=computational.inertia_kg_m2;
  entry.total_mass_kg=physical.mass_kg*static_cast<double>(bin.physical_count);
  const double implied=computational.mass_kg*static_cast<double>(entry.computational_count);
  if(!std::isfinite(entry.total_mass_kg)||entry.total_mass_kg<=0||!std::isfinite(implied)||std::abs(implied-entry.total_mass_kg)>1e-12*entry.total_mass_kg)throw std::invalid_argument("representative mass consistency/overflow");
  result.physical_count+=bin.physical_count;result.computational_count+=entry.computational_count;result.total_mass_kg+=entry.total_mass_kg;
  if(!std::isfinite(result.total_mass_kg))throw std::invalid_argument("inventory mass overflow");
  auto& group=result.groups[bin.group];group.physical_count+=bin.physical_count;group.computational_count+=entry.computational_count;group.mass_kg+=entry.total_mass_kg;
  result.bins.push_back(entry);
 }
 return result;
}
struct RepresentativeContact {
 MaterialContact physical,computational;
 std::uint64_t represented_count_per_particle=1;
 double length_scale=1;
 RepresentativeScope scope=RepresentativeScope::DryBulkCandidate;
 // True means a dimensionally consistent candidate, NOT validated bulk equivalence.
 bool requires_bulk_validation=false;
};
inline RepresentativeContact representative_contact(const MaterialContact& physical,std::uint64_t q,RepresentativeScope scope,bool requests_drag=false){
 const auto length=representative_length_scale(q);validate_contact_model(physical);
 if(scope!=RepresentativeScope::DryBulkCandidate&&scope!=RepresentativeScope::ClearanceOrClogging&&scope!=RepresentativeScope::SizeSegregation&&scope!=RepresentativeScope::ImpactOrDeformation)throw std::invalid_argument("unknown representative scope");
 if(requests_drag)throw std::invalid_argument("drag is not implemented, including ratio one");
 RepresentativeContact result{physical,physical,q,length,scope,q!=1};if(q==1)return result;
 if(scope!=RepresentativeScope::DryBulkCandidate)throw std::invalid_argument("representative scaling forbidden in this scope");
 if(physical.law!=ContactLaw::Linear||physical.rolling.mu!=0||physical.linear.rolling.mu!=0||physical.adhesion_gamma_j_m2!=0||physical.linear.adhesion_gamma_j_m2!=0)throw std::invalid_argument("only dry nonrolling linear contact scaling is contracted");
 // Constant-density, relative-overlap/stress candidate: m~L^3, k~L, c~L^2.
 // Damping ratio c/sqrt(k*m) is invariant. Fixed-gravity bulk similarity is NOT.
 result.computational.linear.kn*=length;result.computational.linear.kt*=length;
 result.computational.linear.cn*=length*length;result.computational.linear.ct*=length*length;
 validate_contact_model(result.computational);return result;
}
inline std::string representative_inventory_csv(const RepresentativeInventory& value){
 std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(17);
 s<<"group,bin,physical_diameter_m,density_kg_m3,physical_count,represented_count_per_particle,computational_count,length_scale,computational_diameter_m,physical_particle_mass_kg,computational_particle_mass_kg,computational_inertia_kg_m2,total_mass_kg\n";
 for(const auto& b:value.bins)s<<b.physical.group<<','<<b.physical.bin<<','<<b.physical.diameter_m<<','<<b.physical.density_kg_m3<<','<<b.physical.physical_count<<','<<b.represented_count_per_particle<<','<<b.computational_count<<','<<b.length_scale<<','<<b.computational_diameter_m<<','<<b.physical_particle_mass_kg<<','<<b.computational_particle_mass_kg<<','<<b.computational_inertia_kg_m2<<','<<b.total_mass_kg<<'\n';
 return s.str();
}
}
