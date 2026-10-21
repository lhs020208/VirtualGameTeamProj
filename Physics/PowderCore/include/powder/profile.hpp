#pragma once
#include <array>
#include <cstdint>
#include <chrono>
namespace powder {
// Opt-in diagnostics only; clocks/counters never enter force or acceptance calculations.
struct ClumpProfile {
 static constexpr std::array<const char*,9> names{{"validation_prepare","pair_search_and_force","primitive_walls","static_mesh_query_and_force","moving_mesh_query_and_force","step_limit","integration","post_step_guards","commit"}};
 std::array<double,9> seconds{};
 std::uint64_t attempted_steps=0,accepted_steps=0,pair_distance_checks=0,post_pair_checks=0,material_pair_validations=0;
 std::uint64_t property_rebuilds=0,property_hits=0,material_table_rebuilds=0,sphere_integrations=0,generic_integrations=0;
 std::uint64_t neighbor_rebuilds=0,neighbor_reuses=0,neighbor_capacity_bytes=0,workspace_capacity_bytes=0;
 std::uint64_t wall_pose_builds=0,wall_triangle_checks=0,wall_analytic_queries=0,wall_bounds_rejects=0;
 double total_seconds()const{double s=0;for(double x:seconds)s+=x;return s;}
};
class ClumpProfileScope {
 using Clock=std::chrono::steady_clock;
 ClumpProfile* profile_;Clock::time_point last_;std::size_t phase_=0;
public:
 explicit ClumpProfileScope(ClumpProfile* p):profile_(p){if(profile_){++profile_->attempted_steps;last_=Clock::now();}}
 void phase(std::size_t next){if(profile_){auto now=Clock::now();profile_->seconds[phase_]+=std::chrono::duration<double>(now-last_).count();last_=now;phase_=next;}}
 void accepted(){if(profile_)++profile_->accepted_steps;}
 ~ClumpProfileScope(){if(profile_)profile_->seconds[phase_]+=std::chrono::duration<double>(Clock::now()-last_).count();}
};
}
