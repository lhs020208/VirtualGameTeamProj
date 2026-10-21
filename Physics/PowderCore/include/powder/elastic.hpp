#pragma once
#include "powder/friction.hpp"
namespace powder {
enum class ContactLaw { Linear, HertzMindlin };
struct ElasticMaterial { double young_pa=1e7, poisson=.25; };
struct MaterialContact {
    ContactLaw law=ContactLaw::HertzMindlin;
    FrictionModel linear;
    ElasticMaterial a,b; // b is the wall material for a plane.
    double cn=0,ct=0,mu=.3; // Ns/m, Ns/m, dimensionless; not restitution coefficients.
    double max_overlap_radius_ratio=.01;
};
struct HertzResponse {
    double effective_young_pa=0,effective_shear_pa=0,elastic_force_n=0;
    double normal_derivative_n_m=0,tangent_stiffness_n_m=0,elastic_energy_j=0;
};
ContactWrench evaluate_material_plane(const Particle& p,Vec3 n,double offset,const MaterialContact& m,ContactHistory& h,double dt);
ContactWrench evaluate_material_pair(const Particle& a,const Particle& b,const MaterialContact& m,ContactHistory& h,double dt);
HertzResponse hertz_response(ElasticMaterial a,ElasticMaterial b,double effective_radius_m,double overlap_m);
FrictionResponse step_material_plane(Particle& p,ForceAccumulator& f,Vec3 normal,double offset,
    const MaterialContact& model,ContactHistory& history,double dt);
FrictionResponse step_material_pair(Particle& a,Particle& b,ForceAccumulator& fa,ForceAccumulator& fb,
    const MaterialContact& model,ContactHistory& history,double dt);
}
