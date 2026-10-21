#pragma once
#include "powder/geometry.hpp"
#include <memory>
#include <string>
namespace powder {
using MeshEdge=std::array<std::size_t,2>;
enum class BoundaryKind { Inlet, Outlet, Perimeter };
struct MeshBoundary {MeshEdge edge;BoundaryKind kind=BoundaryKind::Perimeter;};
struct MeshInput {
 std::vector<Vec3> vertices;
 std::vector<std::array<std::size_t,3>> triangles;
 std::vector<MeshBoundary> boundaries; // Every open edge must be deliberately classified.
 double metres_per_unit=1;
 double minimum_diagonal_m=0,maximum_diagonal_m=0; // Independent expected-size contract, required.
 bool closed_solid=false; // Closed, outward oriented volume; otherwise two-sided zero-thickness sheet.
};
struct MeshDiagnostics {
 std::size_t vertices=0,triangles=0,patches=0,inlet_edges=0,outlet_edges=0,perimeter_edges=0;
 double diagonal_m=0,minimum_edge_m=0;
 bool two_sided_sheet=false;
 std::string text()const;
};
struct MeshHit {std::uint64_t feature_id=0;Vec3 point_m,normal;double overlap_m=0;};
struct MeshQuery {std::vector<MeshHit> contacts;std::size_t triangle_checks=0;};
// Immutable after construction; copies share the precomputed BVH and seam topology.
class StaticTriangleMesh {
public:
 explicit StaticTriangleMesh(const MeshInput&);
 const MeshDiagnostics& diagnostics()const;
 MeshQuery query(const Particle&,bool brute_force=false)const;
 void guard_motion(Vec3 from,Vec3 to)const; // Reject centre crossing; does not silently teleport or repair.
 std::string serialize()const;
private:
 struct Data;std::shared_ptr<const Data> data_;
};
MeshInput read_triangle_mesh(std::istream&);
MeshInput triangulated_box(const FixedBox&);
using MeshHistoryKey=std::array<std::uint64_t,3>; // particle, mesh array index, welded feature
}
