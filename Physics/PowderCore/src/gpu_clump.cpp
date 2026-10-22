#include "powder/gpu_clump.hpp"
#include "powder/gpu.hpp"
#include <set>
namespace powder { namespace {
void need(bool v,const char* s){if(!v)throw std::invalid_argument(s);}
bool equal(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
GpuWall box(const FixedBox& b){need(b.weld_group==0,"GPU welded boxes unsupported");need(equal(b.axes[0],{1,0,0})&&equal(b.axes[1],{0,1,0})&&equal(b.axes[2],{0,0,1}),"GPU reference box must be axis aligned");GpuWall w;w.kind=GpuWallKind::Box;w.centre=b.centre_m;w.half=b.half_m;return w;}
GpuWall box(const StaticTriangleMesh& mesh){std::istringstream in(mesh.serialize());auto m=read_triangle_mesh(in);need(m.closed_solid,"GPU mesh must be a closed box");Vec3 lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};for(auto& v:m.vertices){v.x*=m.metres_per_unit;v.y*=m.metres_per_unit;v.z*=m.metres_per_unit;lo={std::min(lo.x,v.x),std::min(lo.y,v.y),std::min(lo.z,v.z)};hi={std::max(hi.x,v.x),std::max(hi.y,v.y),std::max(hi.z,v.z)};}
 double areas[6]={};for(auto t:m.triangles){int face=-1;for(unsigned a=0;a<3;++a)for(unsigned side=0;side<2;++side){double low[]={lo.x,lo.y,lo.z},high[]={hi.x,hi.y,hi.z};bool on=true;for(auto i:t){auto v=m.vertices.at(i);double xyz[]={v.x,v.y,v.z};on=on&&xyz[a]==(side?high[a]:low[a]);}if(on){need(face==-1,"degenerate box face");face=static_cast<int>(a*2+side);}}need(face>=0,"GPU supports only axis-aligned box surfaces");auto a=m.vertices[t[0]],b=m.vertices[t[1]],c=m.vertices[t[2]];Vec3 u{b.x-a.x,b.y-a.y,b.z-a.z},v{c.x-a.x,c.y-a.y,c.z-a.z};double cross[]={u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};need(cross[face/2]*(face%2?1:-1)>0,"GPU box outward surface required");areas[face]+=.5*std::hypot(cross[0],cross[1],cross[2]);}
 double ext[]={hi.x-lo.x,hi.y-lo.y,hi.z-lo.z};for(unsigned a=0;a<3;++a)for(unsigned side=0;side<2;++side){double expected=ext[(a+1)%3]*ext[(a+2)%3];need(expected>0&&std::abs(areas[2*a+side]-expected)<=1e-12*expected,"GPU box face area incomplete");}FixedBox b;b.centre_m={(lo.x+hi.x)/2,(lo.y+hi.y)/2,(lo.z+hi.z)/2};b.half_m={(hi.x-lo.x)/2,(hi.y-lo.y)/2,(hi.z-lo.z)/2};return box(b);}
class Backend final:public ClumpBackend {
 std::unique_ptr<GpuSpheres> gpu_;bool moving_=false;
 Backend(std::unique_ptr<GpuSpheres> gpu,bool moving):gpu_(std::move(gpu)),moving_(moving){}
public:
 explicit Backend(const ClumpWorld& w):gpu_(std::make_unique<GpuSpheres>()){
 need(w.physical_time_s==0&&w.histories()==0&&w.moving_wall_work_j==0,"GPU initialization requires a fresh state");need(!w.bodies.empty(),"empty GPU world");std::vector<Particle> particles;std::set<std::uint32_t> types;
 for(const auto& b:w.bodies){need(b.pebbles.size()==1&&equal(b.pebbles[0].local_m,{}),"GPU supports centered single spheres only");Particle a;a.id=b.id;a.diameter_m=b.pebbles[0].diameter_m;a.material.density_kg_m3=b.density_kg_m3;a.position_m=b.position_m;a.velocity_m_s=b.velocity_m_s;a.orientation=b.orientation;a.angular_velocity_rad_s=clump_omega(b);particles.push_back(a);types.insert(b.type);}
 auto model=w.pair_models.at({*types.begin(),*types.begin()});auto same=[&](const MaterialContact& m){validate_contact_model(m);need(m.law==ContactLaw::Linear&&m.rolling.mu==0&&m.linear.rolling.mu==0&&m.adhesion_gamma_j_m2==0&&m.linear.adhesion_gamma_j_m2==0,"GPU requires dry nonrolling linear contacts");const auto& a=model.linear;const auto& b=m.linear;need(a.kn==b.kn&&a.cn==b.cn&&a.kt==b.kt&&a.ct==b.ct&&a.mu==b.mu,"GPU requires identical effective pair/wall coefficients");};
 for(auto a:types){same(w.wall_models.at(a));for(auto b:types)if(a<=b)same(w.pair_models.at({a,b}));}
 std::vector<GpuWall> walls;for(auto plane:w.planes){GpuWall b;b.normal=plane.normal;b.offset=plane.offset_m;walls.push_back(b);}for(const auto& b:w.boxes)walls.push_back(box(b));for(const auto& m:w.meshes)walls.push_back(box(m));for(const auto& m:w.moving_meshes){auto b=box(m.reference());const auto& motion=m.motion();b.kind=GpuWallKind::MovingBox;b.centre=motion.point(b.centre,0);b.orientation=motion.rotation(0);b.pivot=motion.pivot(0);b.omega=motion.omega();b.velocity=motion.linear_velocity_m_s;walls.push_back(b);moving_=true;}
 gpu_->reset(particles);gpu_->enable_contacts(model);gpu_->set_walls(walls);
 }
 std::shared_ptr<ClumpBackend> clone()override{return std::shared_ptr<ClumpBackend>(new Backend(gpu_->clone(),moving_));}
 void advance(ClumpWorld& w,double dt)override{gpu_->advance_contacts(dt,1,w.gravity_m_s2);w.physical_time_s=gpu_->physical_time_s();}
 void synchronize(ClumpWorld& w,ClumpReport& report)override{auto p=gpu_->snapshot();need(p.size()==w.bodies.size(),"GPU bridge body count changed");report={};for(std::size_t i=0;i<p.size();++i){auto& b=w.bodies[i];const auto& a=p[i];need(b.id==a.id,"GPU bridge ID changed");b.position_m=a.position_m;b.velocity_m_s=a.velocity_m_s;b.orientation=a.orientation;auto props=sphere_properties(a.diameter_m,a.material);report.mass_kg+=props.mass_kg;b.angular_momentum_world={props.inertia_kg_m2*a.angular_velocity_rad_s.x,props.inertia_kg_m2*a.angular_velocity_rad_s.y,props.inertia_kg_m2*a.angular_velocity_rad_s.z};}w.physical_time_s=gpu_->physical_time_s();if(w.physical_time_s>0){auto r=gpu_->contact_report();report.pair_contacts=r.directed_contacts/2;report.wall_contacts=r.wall_contacts;report.max_overlap_m=r.max_overlap_m;}if(moving_){auto r=gpu_->wall_report();w.moving_wall_work_j=r.work_to_particles_j;report.moving_wall_work_j=r.work_to_particles_j;report.moving_wall_loads.push_back({r.reaction_force_n,r.reaction_torque_nm,r.power_to_particles_w});}}
};
}
std::shared_ptr<ClumpBackend> make_gpu_backend(const ClumpWorld& w){return std::make_shared<Backend>(w);}
}
