#pragma once
#include "powder/mesh.hpp"
#include "powder/elastic.hpp"
namespace powder {
struct PrescribedMotion {
 Vec3 pivot_m,linear_velocity_m_s,axis{0,0,1};
 double rpm=0,phase_rad=0;
 void validate()const;
 Vec3 omega()const;
 Quaternion rotation(double time_s)const;
 Vec3 pivot(double time_s)const;
 Vec3 point(Vec3 reference,double time_s)const;
 Vec3 inverse_point(Vec3 world,double time_s)const;
 Vec3 vector(Vec3 reference,double time_s)const;
 Vec3 velocity(Vec3 world_point,double time_s)const;
};
// Immutable values shared by all particle queries at one prescribed time.
class PreparedMotion {
 Quaternion q_;Vec3 reference_pivot_,pivot_,omega_,linear_;
public:
 PreparedMotion(const PrescribedMotion&,double time_s);
 Vec3 point(Vec3)const;Vec3 inverse_point(Vec3)const;Vec3 vector(Vec3)const;Vec3 velocity(Vec3)const;
 Vec3 omega()const{return omega_;}Vec3 pivot()const{return pivot_;}
};
struct MovingMeshHit {MeshHit geometry;Vec3 velocity_m_s,omega_rad_s,pivot_m;};
struct WallExchange {Vec3 reaction_force_n,reaction_torque_nm;double power_to_particles_w=0;};
struct MovingWallWrench {ContactWrench particle;WallExchange wall;};
MovingWallWrench evaluate_moving_wall(const Particle&,const MovingMeshHit&,const MaterialContact&,ContactHistory&,double dt);
class MovingTriangleMesh {
public:
 MovingTriangleMesh(StaticTriangleMesh,PrescribedMotion);
 const StaticTriangleMesh& reference()const{return reference_;}
 const PrescribedMotion& motion()const{return motion_;}
 std::vector<MovingMeshHit> query(const Particle&,double time_s,MeshQuery* counters=nullptr)const;
 std::vector<MovingMeshHit> query_prepared(const Particle&,const PreparedMotion&,MeshQuery* counters=nullptr)const;
 void guard_prepared(Vec3 from,Vec3 to,const PreparedMotion& start,const PreparedMotion& end)const;
 double step_limit(double minimum_radius_m)const; // Existing travel/angular guards, diagnostic only.
 void validate_step(double time_s,double dt,double minimum_radius_m)const;
 void guard_motion(Vec3 from,Vec3 to,double time_s,double dt)const;
private:
 StaticTriangleMesh reference_;PrescribedMotion motion_;double radius_bound_m_=0;
};
}
