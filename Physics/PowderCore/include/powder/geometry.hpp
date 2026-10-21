#pragma once
#include "powder/core.hpp"
#include <array>
#include <istream>
#include <vector>
namespace powder {
// Static oriented box. Axes are orthonormal world-space columns (reflections allowed).
struct FixedBox {
    std::uint64_t id=0, weld_group=0;
    Vec3 centre_m, half_m;
    std::array<Vec3,3> axes{{{1,0,0},{0,1,0},{0,0,1}}};
};
struct BoxContact {
    std::uint64_t surface_id=0;
    Vec3 point_m, normal;
    double overlap_m=0;
    int feature=0; // 1 face, 2 edge, 3 corner
};
void validate_boxes(const std::vector<FixedBox>& boxes);
std::vector<BoxContact> box_contacts(const Particle& p,const std::vector<FixedBox>& boxes);
// Versioned whitespace format exported by Scripts/export_dem_geometry.py. SI units.
std::vector<FixedBox> read_boxes(std::istream& input);
}
