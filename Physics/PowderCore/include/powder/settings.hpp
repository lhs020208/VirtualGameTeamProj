#pragma once
#include "powder/world.hpp"
#include <string>
#include <functional>
namespace powder {
enum class SizeDistribution { Mono, UniformNumber };
enum class SupplyMode { Batch, Timed };
enum class AmountMode { Count, Mass };
struct ContactSettings {
 FrictionModel linear{4e6,40,1e6,10,.3};
 double cn=40,ct=10,mu=.3,overlap_cap=.01;
};
struct PowderSettings {
 SupplyMode supply_mode=SupplyMode::Batch;
 double supply_start_s=0,supply_interval_s=.01,retry_interval_s=.001;
 std::uint64_t supply_per_event=1;
 SizeDistribution distribution=SizeDistribution::Mono;
 double diameter_min_m=.04,diameter_max_m=.04,density_kg_m3=2500;
 AmountMode amount=AmountMode::Count;
 std::uint64_t count=300;
 double target_mass_kg=0,mass_tolerance_kg=0;
 std::uint64_t seed=42,max_candidates=100000;
 ContactLaw law=ContactLaw::Linear;
 ElasticMaterial particle_material{1e7,.25},wall_material{2e11,.3};
 ContactSettings pair,wall;
 Vec3 centre_m{0,0,4.85},half_m{.185,.185,.35};
 Vec3 velocity_m_s{},gravity_m_s2{0,0,-9.81};
 double spacing_factor=1.25,wall_clearance_m=0,dt_s=2e-7;
};
struct InitialReport {
 bool configured=false;
 std::uint64_t seed=0,count=0,candidates=0;
 double mass_kg=0,target_mass_kg=0,mass_error_kg=0,min_diameter_m=0,max_diameter_m=0;
};
struct InitialBatch {ParticleSystem world;InitialReport report;};
InitialBatch make_inventory(const PowderSettings& settings,ParticleSystem geometry);
// Appends a nonoverlapping prefix, returns admitted count; caller owns transaction.
std::size_t place_supply(const PowderSettings&,ParticleSystem&,const std::vector<Particle>&,double max_diameter,const std::function<bool()>& cancelled={},std::uint64_t* attempts=nullptr);
void validate_settings(const PowderSettings& settings);
std::string serialize_settings(const PowderSettings& settings);
PowderSettings parse_settings(const std::string& text);
// Atomic result: failure throws, never returns a partially populated world.
// Seeded lattice_v1 with fixed bit mapping, not a random close-packing algorithm.
InitialBatch make_initial(const PowderSettings& settings,ParticleSystem geometry,const std::function<bool()>& cancelled={});
}
