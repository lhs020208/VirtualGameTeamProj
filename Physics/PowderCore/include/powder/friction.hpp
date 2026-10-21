#pragma once
#include "powder/core.hpp"
namespace powder {
struct RollingModel { double stiffness_n_m=1000, damping_ns_m=1, mu=0; };
void validate_rolling_model(const RollingModel& model);
struct FrictionModel { double kn=10000, cn=40, kt=2500, ct=10, mu=.3; RollingModel rolling; double adhesion_gamma_j_m2=0; };
// Contact-only constant attraction; gamma is one-surface energy (work of adhesion = 2 gamma).
struct AdhesionResponse {double attraction_n=0,potential_j=0;};
AdhesionResponse contact_adhesion(double gamma_j_m2,double effective_radius_m,double overlap_m);
struct ContactHistory {
    bool active=false, plane=false;
    ParticleId a=0,b=0;
    Vec3 normal, displacement_m;
    Vec3 rolling_displacement_m; // R_eff * integrated relative tangent angular velocity.
};
struct FrictionResponse { Vec3 tangent_force_n; bool sliding=false; double normal_force_n=0; };
struct ContactWrench { FrictionResponse response; Vec3 force_a,force_b,torque_a,torque_b; };
ContactWrench evaluate_friction_plane(const Particle& p, Vec3 normal, double offset,
    const FrictionModel& model, ContactHistory& history, double dt,Vec3 wall_velocity={},Vec3 wall_omega={});
ContactWrench evaluate_friction_pair(const Particle& a,const Particle& b,
    const FrictionModel& model,ContactHistory& history,double dt);
// World-space torque, isotropic solid sphere inertia. Atomic on failure.
void integrate_rigid(Particle& p, ForceAccumulator& forces, Vec3 torque_nm, double dt);
// Infinite static plane dot(normal,x)=offset. History belongs to this one plane.
FrictionResponse step_friction_plane(Particle& p, ForceAccumulator& forces,
    Vec3 normal,double offset,const FrictionModel& model,ContactHistory& history,double dt);
// Canonical caller ordering must remain unchanged while contact is active.
FrictionResponse step_friction_pair(Particle& a,Particle& b,ForceAccumulator& fa,
    ForceAccumulator& fb,const FrictionModel& model,ContactHistory& history,double dt);
}
