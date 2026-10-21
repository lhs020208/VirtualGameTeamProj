#pragma once
#include "powder/elastic.hpp"
#include "powder/geometry.hpp"
#include <map>
#include <optional>
#include <utility>
#include <vector>
namespace powder {
using PairKey=std::pair<ParticleId,ParticleId>;
struct SearchResult { std::vector<PairKey> contacts; std::size_t distance_checks=0; };
SearchResult find_contacts(const std::vector<Particle>& particles,bool brute_force=false);
struct FixedPlane { std::uint64_t id=0; Vec3 normal{0,0,1};double offset_m=0; };
struct StepReport { std::size_t pair_contacts=0,wall_contacts=0,distance_checks=0;double max_overlap_m=0,total_mass_kg=0,max_speed_m_s=0; };
class ParticleSystem {
public:
    // Reconfiguration requires reset_history. Models are shared by all contacts in task 07.
    std::vector<Particle> particles;
    std::vector<FixedPlane> planes;
    std::vector<FixedBox> boxes;
    MaterialContact model; // Particle-particle contact.
    std::optional<MaterialContact> wall_model; // If unset, legacy shared model is retained.
    Vec3 gravity_m_s2{0,0,-9.81};
    StepReport step(double dt,bool brute_force=false);
    void reset_history() { pairs_.clear();walls_.clear();boxes_history_.clear(); }
    std::size_t active_pair_histories() const {return pairs_.size();}
    std::size_t active_wall_histories() const {return walls_.size()+boxes_history_.size();}
private:
    std::map<PairKey,ContactHistory> pairs_,walls_,boxes_history_;
};
}
