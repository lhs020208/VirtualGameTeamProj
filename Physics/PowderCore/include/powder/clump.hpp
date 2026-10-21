#pragma once
#include "powder/world.hpp"
#include <array>
#include <string>
namespace powder {
struct Tensor3 {std::array<double,9> v{};};
struct Pebble {Vec3 local_m;double diameter_m=.04;};
struct ClumpProperties {double mass_kg=0,bounding_radius_m=0;Vec3 centroid_m;Tensor3 inertia_body, inverse_inertia_body;};
// First implementation: nonoverlapping solid spheres with ideal massless rigid connections.
ClumpProperties clump_properties(const std::vector<Pebble>&,double density_kg_m3);
struct ClumpBody {
 ParticleId id=0;std::uint32_t type=0;double density_kg_m3=2500;
 std::vector<Pebble> pebbles;
 Vec3 position_m,velocity_m_s,angular_momentum_world;
 Quaternion orientation;
};
ClumpBody make_clump(ParticleId,std::uint32_t,const std::vector<Pebble>&,double density);
Vec3 clump_omega(const ClumpBody&);
std::vector<Particle> clump_particles(const ClumpBody&);
void integrate_clump(ClumpBody&,Vec3 force_n,Vec3 torque_nm,double dt);
double clump_kinetic_energy(const ClumpBody&);
struct ClumpReport {std::size_t pair_contacts=0,wall_contacts=0;double mass_kg=0,max_overlap_m=0;std::vector<WallExchange> moving_wall_loads;double moving_wall_work_j=0;};
class ClumpWorld {
public:
 std::vector<ClumpBody> bodies;
 std::vector<FixedBox> boxes;
 std::vector<StaticTriangleMesh> meshes;
    std::vector<MovingTriangleMesh> moving_meshes;
    double physical_time_s=0,moving_wall_work_j=0;
 std::vector<FixedPlane> planes;
 Vec3 gravity_m_s2{0,0,-9.81};
 // Explicit canonical type-pair table; no guessed coefficient mixing.
 std::map<std::pair<std::uint32_t,std::uint32_t>,MaterialContact> pair_models;
 std::map<std::uint32_t,MaterialContact> wall_models;
 ClumpReport step(double dt);
 void reset_history(){pairs_.clear();walls_.clear();box_history_.clear();mesh_history_.clear();moving_history_.clear();}
 std::size_t histories()const{return pairs_.size()+walls_.size()+box_history_.size()+mesh_history_.size()+moving_history_.size();}
private:
 std::map<PairKey,ContactHistory> pairs_,walls_,box_history_;
 std::map<MeshHistoryKey,ContactHistory> mesh_history_,moving_history_;
};
// Isolated task-18 fixture. Sphere powder and denser two-pebble inclusions; SI coordinates.
ClumpWorld clump_demo(double powder_density=2500,double inclusion_density=7800);
std::string serialize_clump_state(const ClumpWorld&);
}
