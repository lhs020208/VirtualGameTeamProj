#pragma once
#include "powder/elastic.hpp"
#include <utility>
#include <array>
#include <memory>
#include <string>
namespace powder {
struct GpuDeviceInfo {std::string name,version;std::uint64_t global_bytes=0,max_allocation_bytes=0,fp64_config=0;};
struct GpuMemoryPlan {std::uint64_t state_bytes=0,sort_bytes=0,history_bytes=0,total_bytes=0;};
enum class GpuWallKind { Plane=0, Box=1, MovingBox=2 };
struct GpuWall {GpuWallKind kind=GpuWallKind::Plane;Vec3 centre,half,normal{0,0,1},pivot,omega,velocity;Quaternion orientation;double offset=0;};
struct GpuWallReport {Vec3 reaction_force_n,reaction_torque_nm;double work_to_particles_j=0,power_to_particles_w=0;};
struct GpuContactReport {std::size_t directed_contacts=0,wall_contacts=0;double max_overlap_m=0,min_dt_limit_s=0;};
struct GpuTiming {double upload_wall_s=0,upload_gpu_s=0,compute_wall_s=0,compute_gpu_s=0,download_wall_s=0,download_gpu_s=0,sort_gpu_s=0,contact_gpu_s=0,guard_gpu_s=0,commit_gpu_s=0;};
struct GpuCell {std::array<std::uint64_t,2> count{};std::array<double,2> mass{};};
struct GpuObservationConfig {Vec3 lower{0,0,0},upper{1,1,1};std::array<unsigned,3> grid{{2,2,2}};};
struct GpuObservation {GpuContactReport contacts;GpuWallReport wall;Vec3 momentum;double mass_kg=0,kinetic_j=0,max_speed=0;std::uint64_t count=0;std::vector<GpuCell> cells;GpuCell outside;};
GpuMemoryPlan gpu_memory_plan(std::size_t bodies,unsigned history_slots=0);
// Explicit Windows/OpenCL FP64 backend. No implicit CPU fallback or model conversion.
class GpuSpheres {
public:
 explicit GpuSpheres(std::size_t memory_budget_bytes=512ull*1024*1024);
 ~GpuSpheres();GpuSpheres(const GpuSpheres&)=delete;GpuSpheres& operator=(const GpuSpheres&)=delete;
 GpuDeviceInfo device()const;
 GpuTiming timing()const;
 void set_sort_optimizations(bool skip_unchanged, bool local_merge); // Reference switches for validation.
 void set_spatial_reuse(bool enabled); // false keeps forced-sort/binary-search reference path.
 void set_profiling(bool enabled); // Per-kernel events; max 64 contact steps per batch when enabled.
 void reset(const std::vector<Particle>&);
 // No-contact, uniform external force per body; contact scenes must use contact mode.
 void advance_free(double dt,std::uint32_t steps,Vec3 gravity={0,0,-9.81},Vec3 force={});
 void enable_contacts(const MaterialContact&,unsigned history_slots=32,unsigned candidate_limit=256);
 void set_walls(const std::vector<GpuWall>&);
 GpuWallReport wall_report();
 void set_observation(const GpuObservationConfig&,const std::vector<unsigned>& groups);
 GpuObservation observe(); // Deterministic FP64 reductions; no full-state download.
 std::unique_ptr<GpuSpheres> clone();
 void advance_contacts(double dt,std::uint32_t steps,Vec3 gravity={0,0,-9.81});
 GpuContactReport contact_report();
 std::vector<std::pair<ParticleId,ParticleId>> contact_pairs();
 void replace(const std::vector<Particle>&); // Preserve surviving histories; existing states must match snapshot.
 std::vector<Particle> snapshot();
 double physical_time_s()const;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
