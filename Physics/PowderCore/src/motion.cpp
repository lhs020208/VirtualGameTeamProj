#include "powder/motion.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <stdexcept>
namespace powder {namespace {
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 mul(Vec3 a,double s){return {a.x*s,a.y*s,a.z*s};}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double norm(Vec3 a){return std::hypot(a.x,a.y,a.z);}
void require(bool b,const char* s){if(!b)throw std::invalid_argument(s);}
void finite(double x){require(std::isfinite(x),"nonfinite prescribed motion or wall exchange");}
void finite(Vec3 a){finite(a.x);finite(a.y);finite(a.z);}
Vec3 rotate(Quaternion q,Vec3 v){auto u=Vec3{q.x,q.y,q.z};return add(v,add(mul(cross(u,v),2*q.w),mul(cross(u,cross(u,v)),2)));}
}
void PrescribedMotion::validate()const{finite(pivot_m);finite(linear_velocity_m_s);finite(axis);finite(rpm);finite(phase_rad);require(std::abs(norm(axis)-1)<1e-12,"motion axis must be unit");finite(rpm*2*3.14159265358979323846/60);}
Vec3 PrescribedMotion::omega()const{validate();return mul(axis,rpm*2*3.14159265358979323846/60);}
Quaternion PrescribedMotion::rotation(double t)const{validate();finite(t);require(t>=0,"negative motion time");double angle=phase_rad+rpm*2*3.14159265358979323846/60*t;finite(angle);require(std::abs(angle)<1e9,"motion phase exceeds precision range");double s=std::sin(angle/2);return {std::cos(angle/2),axis.x*s,axis.y*s,axis.z*s};}
Vec3 PrescribedMotion::pivot(double t)const{rotation(t);auto p=add(pivot_m,mul(linear_velocity_m_s,t));finite(p);return p;}
Vec3 PrescribedMotion::point(Vec3 p,double t)const{finite(p);return add(pivot(t),rotate(rotation(t),add(p,mul(pivot_m,-1))));}
Vec3 PrescribedMotion::inverse_point(Vec3 p,double t)const{finite(p);auto q=rotation(t);return add(pivot_m,rotate({q.w,-q.x,-q.y,-q.z},add(p,mul(pivot(t),-1))));}
Vec3 PrescribedMotion::vector(Vec3 p,double t)const{finite(p);return rotate(rotation(t),p);}
Vec3 PrescribedMotion::velocity(Vec3 p,double t)const{finite(p);auto v=add(linear_velocity_m_s,cross(omega(),add(p,mul(pivot(t),-1))));finite(v);return v;}
PreparedMotion::PreparedMotion(const PrescribedMotion& m,double t):q_(m.rotation(t)),reference_pivot_(m.pivot_m),pivot_(m.pivot(t)),omega_(m.omega()),linear_(m.linear_velocity_m_s){}
Vec3 PreparedMotion::point(Vec3 p)const{finite(p);return add(pivot_,rotate(q_,add(p,mul(reference_pivot_,-1))));}
Vec3 PreparedMotion::inverse_point(Vec3 p)const{finite(p);return add(reference_pivot_,rotate({q_.w,-q_.x,-q_.y,-q_.z},add(p,mul(pivot_,-1))));}
Vec3 PreparedMotion::vector(Vec3 p)const{finite(p);return rotate(q_,p);}
Vec3 PreparedMotion::velocity(Vec3 p)const{finite(p);auto v=add(linear_,cross(omega_,add(p,mul(pivot_,-1))));finite(v);return v;}
MovingTriangleMesh::MovingTriangleMesh(StaticTriangleMesh mesh,PrescribedMotion motion):reference_(std::move(mesh)),motion_(motion){motion_.validate();radius_bound_m_=reference_.radius_bound(motion_.pivot_m);finite(radius_bound_m_);}
std::vector<MovingMeshHit> MovingTriangleMesh::query(const Particle& p,double t,MeshQuery* counters)const{auto local=p;local.position_m=motion_.inverse_point(p.position_m,t);std::vector<MovingMeshHit> out;auto query=reference_.query(local);if(counters){counters->triangle_checks=query.triangle_checks;counters->analytic_queries=query.analytic_queries;counters->bounds_rejects=query.bounds_rejects;}for(auto c:query.contacts){c.point_m=motion_.point(c.point_m,t);c.normal=motion_.vector(c.normal,t);out.push_back({c,motion_.velocity(c.point_m,t),motion_.omega(),motion_.pivot(t)});}return out;}
std::vector<MovingMeshHit> MovingTriangleMesh::query_prepared(const Particle& p,const PreparedMotion& pose,MeshQuery* counters)const{
 auto local=p;local.position_m=pose.inverse_point(p.position_m);auto query=reference_.query_optimized(local);if(counters){counters->triangle_checks=query.triangle_checks;counters->analytic_queries=query.analytic_queries;counters->bounds_rejects=query.bounds_rejects;}
 std::vector<MovingMeshHit> out;out.reserve(query.contacts.size());for(auto c:query.contacts){c.point_m=pose.point(c.point_m);c.normal=pose.vector(c.normal);out.push_back({c,pose.velocity(c.point_m),pose.omega(),pose.pivot()});}return out;
}
void MovingTriangleMesh::guard_prepared(Vec3 from,Vec3 to,const PreparedMotion& start,const PreparedMotion& end)const{reference_.guard_motion(start.inverse_point(from),end.inverse_point(to));}
void MovingTriangleMesh::validate_step(double t,double dt,double r)const{finite(t);finite(dt);finite(r);require(dt>0&&r>0,"invalid moving mesh step");motion_.rotation(t+dt);double angle=norm(motion_.omega())*dt;require(angle<=.01,"wall angular step exceeds 0.01 radians");double travel=(norm(motion_.linear_velocity_m_s)+norm(motion_.omega())*radius_bound_m_)*dt;require(travel<=.05*r,"wall displacement too large for discrete mesh contact");}
void MovingTriangleMesh::guard_motion(Vec3 from,Vec3 to,double t,double dt)const{reference_.guard_motion(motion_.inverse_point(from,t),motion_.inverse_point(to,t+dt));}
MovingWallWrench evaluate_moving_wall(const Particle& p,const MovingMeshHit& c,const MaterialContact& model,ContactHistory& history,double dt){
 auto h=history;const auto& g=c.geometry;finite(c.velocity_m_s);finite(c.omega_rad_s);finite(c.pivot_m);
 auto w=evaluate_material_plane(p,g.normal,dot(g.normal,g.point_m),model,h,dt,c.velocity_m_s,c.omega_rad_s);
 // Torque at particle centre consists of contact-force arm plus the independent rolling couple.
 auto arm=add(g.point_m,mul(p.position_m,-1));auto rolling=add(w.torque_a,mul(cross(arm,w.force_a),-1));WallExchange wall;
 wall.reaction_force_n=mul(w.force_a,-1);wall.reaction_torque_nm=mul(add(cross(add(g.point_m,mul(c.pivot_m,-1)),w.force_a),rolling),-1);
 wall.power_to_particles_w=dot(w.force_a,c.velocity_m_s)+dot(rolling,c.omega_rad_s);finite(wall.reaction_force_n);finite(wall.reaction_torque_nm);finite(wall.power_to_particles_w);history=h;return {w,wall};
}
}
