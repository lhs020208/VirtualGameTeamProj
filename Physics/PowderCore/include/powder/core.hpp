#pragma once
#include <cstdint>
#include <vector>

namespace powder {
inline constexpr const char* Version = "0.14.0";
inline constexpr const char* Model = "translation_symplectic_euler";
using ParticleId = std::uint64_t;
struct Vec3 { double x=0, y=0, z=0; };
struct Quaternion { double w=1, x=0, y=0, z=0; };
struct Material { double density_kg_m3=1000; };
struct SphereProperties { double radius_m, mass_kg, inertia_kg_m2; };
struct Particle {
    ParticleId id=0;
    double diameter_m=0.001;
    Material material;
    Vec3 position_m, velocity_m_s, angular_velocity_rad_s;
    Quaternion orientation;
};
struct RunConfig {
    double dt_s=0.0001;
    std::uint64_t steps=10000, seed=100;
};
double metres_to_centimetres(double value);
double centimetres_to_metres(double value);
Vec3 metres_to_centimetres(Vec3 value);
Vec3 centimetres_to_metres(Vec3 value);
SphereProperties sphere_properties(double diameter_m, Material material);
void validate(const Particle& particle);
void validate_particles(const std::vector<Particle>& particles);
void validate(const RunConfig& config);
// Forces are reset after each successful step; callers reapply persistent loads.
class ForceAccumulator {
public:
    void add(Vec3 force_n);
    void add_gravity(double mass_kg, Vec3 acceleration_m_s2);
    Vec3 total_n() const noexcept { return total_; }
    void clear() noexcept { total_={}; }
private:
    Vec3 total_;
};
// Translation only. Legacy translation-only API rejects angular velocity; use integrate_rigid for rotation.
// On error, particle and force accumulator are unchanged.
void integrate_translation(Particle& particle, ForceAccumulator& forces, double dt_s);
// Horizontal, immovable, one-sided plane. Compression-only dashpot prevents adhesion.
struct PlaneContact {
    double height_m=0, stiffness_n_m=100, damping_n_s_m=0;
};
struct ContactResult {
    double overlap_m=0, normal_force_n=0, elastic_energy_j=0, dissipation_w=0;
};
void validate(const PlaneContact& plane);
ContactResult plane_contact(const Particle& particle, const PlaneContact& plane);
double contact_step_limit_s(const Particle& particle, const PlaneContact& plane);
void step_with_plane(Particle& particle, ForceAccumulator& forces,
                     const PlaneContact& plane, double dt_s);
struct PairContact { double stiffness_n_m=100, damping_n_s_m=0; };
struct PairResult {
    Vec3 force_on_a_n, force_on_b_n;
    double overlap_m=0, effective_mass_kg=0, elastic_energy_j=0, dissipation_w=0;
};
PairResult sphere_contact(const Particle& a, const Particle& b, const PairContact& model);
double pair_step_limit_s(const Particle& a, const Particle& b, const PairContact& model);
// Evaluates the unordered pair exactly once at the old state. Atomic commit of both bodies.
void step_pair(Particle& a, Particle& b, ForceAccumulator& fa, ForceAccumulator& fb,
               const PairContact& model, double dt_s);
// Integer ticks avoid accumulating dt rounding error. No wall clock is consulted.
class FixedClock {
public:
    explicit FixedClock(RunConfig config);
    bool advance();
    void reset() noexcept { tick_=0; }
    std::uint64_t tick() const noexcept { return tick_; }
    double time_s() const noexcept;
private:
    RunConfig config_;
    std::uint64_t tick_=0;
};
}
