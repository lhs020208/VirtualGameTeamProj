#pragma once
#include <array>
#include <cmath>
#include <stdexcept>
#include <cstdint>
#include <limits>
namespace powder {
// One computational sphere represents its own volume at true density. No free
// statistical mass multiplier: that would change inertia independently of charge.
// Source diameter is metadata only; zero means unknown, never a micron simulation.
struct GranularMaterial {
 double source_diameter_m=0;
 double radius_m=.0063647064, density_kg_m3=1200;
 double particle_friction=.4, wall_friction=.4;
 double mass_kg() const {return 4.188790204786391*radius_m*radius_m*radius_m*density_kg_m3;}
 double represented_mass_kg() const {return mass_kg();}
 double equivalent_source_count() const {return source_diameter_m>0?std::pow(2*radius_m/source_diameter_m,3):0;}
 void validate() const {
  if(!std::isfinite(source_diameter_m)||source_diameter_m<0||!std::isfinite(radius_m)||radius_m<=.001||radius_m>=.03||
     !std::isfinite(density_kg_m3)||density_kg_m3<=0||!std::isnormal(float(mass_kg()))||!std::isnormal(float(1/mass_kg()))||
     !std::isfinite(particle_friction)||particle_friction<0||particle_friction>1||
     !std::isfinite(wall_friction)||wall_friction<0||wall_friction>1)
   throw std::invalid_argument("invalid representative material (SI, radius 1..30 mm exclusive, friction 0..1)");
 }
};
struct BinaryCharge {
 std::array<std::uint64_t,2> counts{};
 std::array<double,2> masses{};
 double requested_mass=0,requested_fraction_a=0;
};
// Nearest whole spheres; expose actual masses instead of silently promising exact
// composition at finite resolution. The caller must check packing and capacity.
inline BinaryCharge plan_binary_charge(double kg,double fraction,const std::array<GranularMaterial,2>& m){
 if(!std::isfinite(kg)||kg<=0||!std::isfinite(fraction)||fraction<0||fraction>1)throw std::invalid_argument("invalid charge");
 BinaryCharge p;p.requested_mass=kg;p.requested_fraction_a=fraction;
 for(int g=0;g<2;++g){m[g].validate();double n=kg*(g==0?fraction:1-fraction)/m[g].mass_kg();
  if(n>1000000)throw std::invalid_argument("charge exceeds supported particle budget");
  p.counts[g]=static_cast<std::uint64_t>(std::floor(n+.5));p.masses[g]=p.counts[g]*m[g].mass_kg();}
 if(p.counts[0]+p.counts[1]==0||p.counts[0]+p.counts[1]>1000000)throw std::invalid_argument("charge resolution/capacity");
 return p;
}
}
