#include "powder/core.hpp"
#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace powder {
namespace {
void finite(double v,const char* name) {
    if(!std::isfinite(v)) throw std::invalid_argument(std::string(name)+" must be finite");
}
void positive(double v,const char* name) {
    finite(v,name);
    if(v<=0) throw std::invalid_argument(std::string(name)+" must be positive");
}
void finite(Vec3 v,const char* name) { finite(v.x,name); finite(v.y,name); finite(v.z,name); }
}
double metres_to_centimetres(double v) {
    finite(v,"metres"); double result=v*100; finite(result,"centimetres"); return result;
}
double centimetres_to_metres(double v) {
    finite(v,"centimetres"); double result=v/100;
    if(v!=0 && result==0) throw std::invalid_argument("metres conversion underflow");
    return result;
}
Vec3 metres_to_centimetres(Vec3 v) { return {metres_to_centimetres(v.x),metres_to_centimetres(v.y),metres_to_centimetres(v.z)}; }
Vec3 centimetres_to_metres(Vec3 v) { return {centimetres_to_metres(v.x),centimetres_to_metres(v.y),centimetres_to_metres(v.z)}; }
SphereProperties sphere_properties(double d,Material m) {
    positive(d,"diameter_m"); positive(m.density_kg_m3,"density_kg_m3");
    constexpr double pi=3.141592653589793238462643383279502884;
    const double r=d/2;
    const double mass=(pi/6)*d*d*d*m.density_kg_m3;
    const double inertia=0.4*mass*r*r;
    positive(r,"derived radius_m"); positive(mass,"derived mass_kg"); positive(inertia,"derived inertia_kg_m2");
    return {r,mass,inertia};
}
void validate(const Particle& p) {
    (void)sphere_properties(p.diameter_m,p.material);
    finite(p.position_m,"position_m"); finite(p.velocity_m_s,"velocity_m_s"); finite(p.angular_velocity_rad_s,"angular_velocity_rad_s");
    const auto q=p.orientation;
    finite(q.w,"orientation"); finite(q.x,"orientation"); finite(q.y,"orientation"); finite(q.z,"orientation");
    const double norm=std::hypot(std::hypot(q.w,q.x),std::hypot(q.y,q.z));
    if(std::abs(norm-1)>1e-12) throw std::invalid_argument("orientation must be a unit quaternion");
}
void validate_particles(const std::vector<Particle>& particles) {
    std::unordered_set<ParticleId> ids;
    for(const auto& p:particles) {
        validate(p);
        if(!ids.insert(p.id).second) throw std::invalid_argument("duplicate particle id");
    }
}
void validate(const RunConfig& c) {
    positive(c.dt_s,"dt_s");
    // All ticks, including a final tick, must be exactly representable as double.
    if(c.steps>9007199254740992ULL) throw std::invalid_argument("steps exceeds exact tick range (2^53)");
    const double end=static_cast<double>(c.steps)*c.dt_s;
    finite(end,"end_time_s");
    if(c.steps && !(end>static_cast<double>(c.steps-1)*c.dt_s))
        throw std::invalid_argument("dt_s cannot resolve final time increment");
}
FixedClock::FixedClock(RunConfig c):config_(c) { validate(c); }
bool FixedClock::advance() {
    if(tick_==config_.steps) return false;
    ++tick_; return true;
}
double FixedClock::time_s() const noexcept { return static_cast<double>(tick_)*config_.dt_s; }
void ForceAccumulator::add(Vec3 force) {
    finite(force,"force_n");
    const Vec3 next{total_.x+force.x,total_.y+force.y,total_.z+force.z};
    finite(next,"accumulated force_n"); total_=next;
}
void ForceAccumulator::add_gravity(double mass,Vec3 g) {
    positive(mass,"mass_kg"); finite(g,"gravity_m_s2");
    add({mass*g.x,mass*g.y,mass*g.z});
}
void integrate_translation(Particle& p,ForceAccumulator& forces,double dt) {
    validate(p); positive(dt,"dt_s");
    if(p.angular_velocity_rad_s.x!=0 || p.angular_velocity_rad_s.y!=0 || p.angular_velocity_rad_s.z!=0)
        throw std::invalid_argument("rotation integration is not implemented");
    const double mass=sphere_properties(p.diameter_m,p.material).mass_kg;
    const Vec3 f=forces.total_n();
    const Vec3 a{f.x/mass,f.y/mass,f.z/mass}; finite(a,"acceleration_m_s2");
    Particle next=p;
    next.velocity_m_s={p.velocity_m_s.x+a.x*dt,p.velocity_m_s.y+a.y*dt,p.velocity_m_s.z+a.z*dt};
    next.position_m={p.position_m.x+next.velocity_m_s.x*dt,p.position_m.y+next.velocity_m_s.y*dt,p.position_m.z+next.velocity_m_s.z*dt};
    validate(next); p=next; forces.clear();
}

void validate(const PlaneContact& plane) {
    finite(plane.height_m,"plane height_m");
    positive(plane.stiffness_n_m,"stiffness_n_m");
    finite(plane.damping_n_s_m,"damping_n_s_m");
    if(plane.damping_n_s_m<0) throw std::invalid_argument("damping must be nonnegative");
}
ContactResult plane_contact(const Particle& p,const PlaneContact& plane) {
    validate(p); validate(plane);
    const auto sphere=sphere_properties(p.diameter_m,p.material);
    const double overlap=plane.height_m+sphere.radius_m-p.position_m.z;
    finite(overlap,"overlap_m");
    if(overlap<=0) return {};
    const double compression_speed=p.velocity_m_s.z<0 ? -p.velocity_m_s.z : 0;
    ContactResult r{overlap,plane.stiffness_n_m*overlap+plane.damping_n_s_m*compression_speed,
        .5*plane.stiffness_n_m*overlap*overlap,plane.damping_n_s_m*compression_speed*compression_speed};
    finite(r.normal_force_n,"normal_force_n"); finite(r.elastic_energy_j,"elastic_energy_j"); finite(r.dissipation_w,"dissipation_w");
    return r;
}
double contact_step_limit_s(const Particle& p,const PlaneContact& plane) {
    validate(p); validate(plane);
    const double mass=sphere_properties(p.diameter_m,p.material).mass_kg;
    const double rate=std::sqrt(plane.stiffness_n_m/mass)+plane.damping_n_s_m/mass;
    positive(rate,"contact frequency");
    const double limit=.02/rate; positive(limit,"contact dt limit"); return limit;
}
void step_with_plane(Particle& p,ForceAccumulator& forces,const PlaneContact& plane,double dt) {
    positive(dt,"dt_s");
    if(dt>contact_step_limit_s(p,plane)) throw std::invalid_argument("dt_s exceeds conservative contact step limit");
    auto pending=forces;
    const auto contact=plane_contact(p,plane);
    pending.add({0,0,contact.normal_force_n});
    integrate_translation(p,pending,dt);
    forces.clear();
}

PairResult sphere_contact(const Particle& a,const Particle& b,const PairContact& model) {
    validate(a); validate(b); validate(PlaneContact{0,model.stiffness_n_m,model.damping_n_s_m});
    if(a.id==b.id) throw std::invalid_argument("pair requires distinct particle ids");
    const auto sa=sphere_properties(a.diameter_m,a.material), sb=sphere_properties(b.diameter_m,b.material);
    const double low=sa.mass_kg<sb.mass_kg ? sa.mass_kg : sb.mass_kg;
    const double high=sa.mass_kg<sb.mass_kg ? sb.mass_kg : sa.mass_kg;
    PairResult r; r.effective_mass_kg=low/(1+low/high);
    positive(r.effective_mass_kg,"effective_mass_kg");
    const Vec3 d{b.position_m.x-a.position_m.x,b.position_m.y-a.position_m.y,b.position_m.z-a.position_m.z};
    finite(d,"pair separation");
    const double distance=std::hypot(d.x,d.y,d.z); positive(distance,"pair centre distance (coincident centres unsupported)");
    const double overlap=sa.radius_m+sb.radius_m-distance; finite(overlap,"pair overlap");
    if(overlap<=0) return r;
    const Vec3 n{d.x/distance,d.y/distance,d.z/distance};
    const Vec3 dv{b.velocity_m_s.x-a.velocity_m_s.x,b.velocity_m_s.y-a.velocity_m_s.y,b.velocity_m_s.z-a.velocity_m_s.z};
    finite(dv,"relative velocity");
    const double normal_speed=dv.x*n.x+dv.y*n.y+dv.z*n.z; finite(normal_speed,"normal speed");
    const double compression=normal_speed<0 ? -normal_speed : 0;
    const double force=model.stiffness_n_m*overlap+model.damping_n_s_m*compression;
    finite(force,"pair normal force");
    r.overlap_m=overlap;
    r.force_on_b_n={force*n.x,force*n.y,force*n.z};
    r.force_on_a_n={-r.force_on_b_n.x,-r.force_on_b_n.y,-r.force_on_b_n.z};
    r.elastic_energy_j=.5*model.stiffness_n_m*overlap*overlap;
    r.dissipation_w=model.damping_n_s_m*compression*compression;
    finite(r.elastic_energy_j,"pair elastic energy"); finite(r.dissipation_w,"pair dissipation");
    return r;
}
double pair_step_limit_s(const Particle& a,const Particle& b,const PairContact& model) {
    const double mass=sphere_contact(a,b,model).effective_mass_kg;
    const double rate=std::sqrt(model.stiffness_n_m/mass)+model.damping_n_s_m/mass;
    positive(rate,"pair contact frequency");
    const double limit=.02/rate; positive(limit,"pair dt limit"); return limit;
}
void step_pair(Particle& a,Particle& b,ForceAccumulator& fa,ForceAccumulator& fb,const PairContact& model,double dt) {
    positive(dt,"dt_s");
    if(&a==&b || &fa==&fb) throw std::invalid_argument("pair state/load aliases are forbidden");
    const auto contact=sphere_contact(a,b,model);
    const double mass=contact.effective_mass_kg;
    const double rate=std::sqrt(model.stiffness_n_m/mass)+model.damping_n_s_m/mass;
    positive(rate,"pair contact frequency");
    if(dt>.02/rate) throw std::invalid_argument("dt_s exceeds conservative pair step limit");
    auto na=a, nb=b; auto pa=fa, pb=fb;
    pa.add(contact.force_on_a_n); pb.add(contact.force_on_b_n);
    integrate_translation(na,pa,dt); integrate_translation(nb,pb,dt);
    a=na; b=nb; fa.clear(); fb.clear();
}

}
