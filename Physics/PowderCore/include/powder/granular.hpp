#pragma once
#include <array>
#include "material.hpp"
#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
namespace powder {
// Indices: clear, build, predict, contact(+boundary if fused), velocity(+probe if fused), projection, probe, readback, candidate build.
struct GranularTiming {
 std::array<double,14> gpu_seconds{};
 std::array<std::uint64_t,14> calls{};
 double advance_wall=0,snapshot_wall=0,submit_wall=0,guard_wait_wall=0,profile_collect_wall=0;
 bool profiling_enabled=false,split_boundaries=false,candidates_enabled=false;
 std::uint64_t candidate_builds=0,candidate_fallback_particle_iterations=0;
 unsigned candidate_capacity=0;
 bool boundary_candidates_enabled=false,kernel_optimize_enabled=false;
 std::uint64_t argument_calls=0,argument_cache_hits=0;
 bool candidate_reuse_enabled=false,candidate_audit_enabled=false;
 double candidate_skin_m=0;std::uint64_t reuse_checks=0,candidate_reuses=0;
};
void write_granular_timing(std::ostream& out,const GranularTiming& timing);
struct Grain {float x,y,z,w;};
struct GranularConfig {bool rotation=false;float rolling_friction=0;bool binary_materials=false;std::array<GranularMaterial,2> materials{};float cross_friction=.4f;unsigned count=30000,iterations=16;bool fast_bounds=true,profile=false,test_mixer=false,profile_split=false,candidates=false,candidate_reuse=false,candidate_audit=false,boundary_candidates=false,kernel_optimize=false;float candidate_skin_ratio=.5f;unsigned candidate_capacity=64;float radius=0,dt=.00025f,friction=.4f;};
struct GranularBoundarySample {Grain position,normal,velocity;unsigned plane_tests=0,rotor_tests=0;};
struct GranularCandidates {std::vector<std::vector<unsigned>> neighbors;unsigned overflow_particles=0;};
void validate_granular_initial(GranularConfig config,const std::vector<Grain>& positions);
// Explicit approximate position-contact solver; persistent IDs = vector indices.
// SI units; legacy equal spheres or opt-in binary mass-weighted spheres and rotation.
// No adhesion or contact-force certification.
class GranularGPU {
public:
 explicit GranularGPU(GranularConfig config);
 GranularGPU(GranularConfig config,const std::vector<Grain>& positions,const std::vector<Grain>& velocities);~GranularGPU();
 GranularGPU(const GranularGPU&)=delete;GranularGPU& operator=(const GranularGPU&)=delete;
 void advance(unsigned steps,float rpm,bool gate_open);
 std::vector<Grain> snapshot();
 std::vector<Grain> velocity_snapshot();
 std::vector<Grain> angular_velocity_snapshot();
 // Single-threaded owner API. One pending transfer; reset/destruction drain it safely.
 void request_snapshot();
 bool receive_snapshot(std::vector<Grain>& destination,bool wait=false);
 void set_state(const std::vector<Grain>& positions,const std::vector<Grain>& velocities);
 // Diagnostic only (<=4096 particles): rebuild and read the effective candidate sets.
 GranularCandidates inspect_candidates();
 // Isolated boundary evaluation; useful for comparing projection AND wall friction data.
 std::vector<GranularBoundarySample> inspect_boundaries(const std::vector<Grain>& points,float angle,bool gate=false,bool velocity_shell=false);
 GranularTiming timing()const;double time()const;float radius()const;float angle()const;std::string device()const;
private:struct Impl;std::unique_ptr<Impl> p;
};
}
