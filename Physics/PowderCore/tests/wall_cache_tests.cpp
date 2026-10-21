#include "powder/clump.hpp"
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace powder;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
void near(double a,double b){check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=1e-12+1e-9*std::max(std::abs(a),std::abs(b)),"wall numerical parity");}
void near(Vec3 a,Vec3 b){near(a.x,b.x);near(a.y,b.y);near(a.z,b.z);}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::invalid_argument&){bad=true;}check(bad,"missing wall rejection");}
Particle particle(Vec3 x,double d=.04){Particle p;p.id=1;p.diameter_m=d;p.material.density_kg_m3=2500;p.position_m=x;p.velocity_m_s={.01,.02,.03};p.angular_velocity_rad_s={.1,.2,.3};return p;}
void compare(const MeshQuery& a,const MeshQuery& b){check(a.contacts.size()==b.contacts.size(),"contact count");for(std::size_t i=0;i<a.contacts.size();++i){check(a.contacts[i].feature_id==b.contacts[i].feature_id,"history feature");near(a.contacts[i].point_m,b.contacts[i].point_m);near(a.contacts[i].normal,b.contacts[i].normal);near(a.contacts[i].overlap_m,b.contacts[i].overlap_m);}}
MeshInput grid(int n){MeshInput m;m.minimum_diagonal_m=5;m.maximum_diagonal_m=6;for(int y=0;y<=n;++y)for(int x=0;x<=n;++x)m.vertices.push_back({-2.+4.*x/n,-2.+4.*y/n,0});for(int y=0;y<n;++y)for(int x=0;x<n;++x){auto a=static_cast<std::size_t>(y*(n+1)+x),b=a+1,c=a+n+1,d=c+1;m.triangles.push_back({a,b,d});m.triangles.push_back({a,d,c});}for(int k=0;k<n;++k){auto i=static_cast<std::size_t>(k),w=static_cast<std::size_t>(n+1);m.boundaries.push_back({{i,i+1},BoundaryKind::Perimeter});m.boundaries.push_back({{n*w+i,n*w+i+1},BoundaryKind::Perimeter});m.boundaries.push_back({{i*w,(i+1)*w},BoundaryKind::Perimeter});m.boundaries.push_back({{i*w+n,(i+1)*w+n},BoundaryKind::Perimeter});}return m;}
MeshInput tube(){MeshInput m;m.minimum_diagonal_m=1;m.maximum_diagonal_m=2;for(double z:{0.,1.})for(Vec3 v:std::vector<Vec3>{{-.1,-.1,z},{.1,-.1,z},{.1,.1,z},{-.1,.1,z}})m.vertices.push_back(v);for(std::size_t i=0;i<4;++i){auto j=(i+1)%4;m.triangles.push_back({i,j,j+4});m.triangles.push_back({i,j+4,i+4});m.boundaries.push_back({{i,j},BoundaryKind::Outlet});m.boundaries.push_back({{i+4,j+4},BoundaryKind::Inlet});}return m;}

int main(){try{
 FixedBox box;box.id=1;box.half_m={.5,.5,.05};StaticTriangleMesh mesh(triangulated_box(box));std::size_t analytic=0;
 for(int axis=0;axis<3;++axis)for(double sign:{-1.,1.})for(int n=0;n<40;++n){Vec3 x{.001*n-.02,.003*n-.04,0};if(axis==0){x.x=sign*.5199;x.z=.001;}if(axis==1){x.y=sign*.5199;x.z=.001;}if(axis==2)x.z=sign*.0699;auto p=particle(x);auto a=mesh.query_optimized(p),b=mesh.query(p,true);compare(a,b);analytic+=a.analytic_queries;}
 check(analytic==240,"box-face path unused");
 for(auto pos:{Vec3{.51,.51,0},Vec3{.51,.51,.06},Vec3{.495,0,.0699},Vec3{0,0,.0699}})compare(mesh.query_optimized(particle(pos)),mesh.query(particle(pos),true));
 auto far=mesh.query_optimized(particle({10,10,10}));check(far.contacts.empty()&&far.bounds_rejects==1&&far.triangle_checks==0,"whole-object rejection");
 rejects([&]{mesh.query_optimized(particle({0,0,0}));});rejects([&]{mesh.query_optimized(particle({0,0,.05}));});rejects([&]{mesh.guard_motion({0,0,.1},{0,0,-.1});});
 auto rotated=box;const double c=std::sqrt(.5);rotated.axes={Vec3{c,c,0},Vec3{-c,c,0},Vec3{0,0,1}};StaticTriangleMesh rotated_mesh(triangulated_box(rotated));auto rq=rotated_mesh.query_optimized(particle({0,0,.0699}));check(rq.analytic_queries==0,"rotated mesh incorrectly certified");compare(rq,rotated_mesh.query(particle({0,0,.0699}),true));
 StaticTriangleMesh sheet(grid(4)),pipe(tube());for(int n=0;n<=100;++n){auto p=particle({-.8+.016*n,.123,.0199});compare(sheet.query_optimized(p),sheet.query(p,true));auto wall=particle({.09,0,.01+.0098*n});compare(pipe.query_optimized(wall),pipe.query(wall,true));check(pipe.query_optimized(particle({0,0,-.1+.012*n})).contacts.empty(),"port blocked");}pipe.guard_motion({0,0,1.1},{0,0,-.1});rejects([&]{pipe.guard_motion({0,0,.5},{.2,0,.5});});
 FixedBox thin=box;thin.half_m.z=1e-5;StaticTriangleMesh thinmesh(triangulated_box(thin));for(double sign:{-1.,1.})compare(thinmesh.query_optimized(particle({0,0,sign*.01})),thinmesh.query(particle({0,0,sign*.01}),true));rejects([&]{thinmesh.guard_motion({0,0,.01},{0,0,-.01});});
 // Shared poses: translation, both RPM signs, phase, contact force/rolling torque/work.
 for(double rpm:{-30.,0.,30.})for(double t:{0.,.1,.3}){PrescribedMotion m;m.pivot_m={0,0,0};m.linear_velocity_m_s={.01,.02,.03};m.rpm=rpm;m.phase_rad=.2;MovingTriangleMesh moving(mesh,m);PreparedMotion pose(m,t),end(m,t+1e-7);auto p=particle(m.point({.1,.12,.0699},t));auto a=moving.query_prepared(p,pose),b=moving.query(p,t);check(a.size()==b.size()&&!a.empty(),"moving contact count");
 for(auto law:{ContactLaw::Linear,ContactLaw::HertzMindlin}){MaterialContact model;model.law=law;model.rolling={1000,1,.1};model.adhesion_gamma_j_m2=.001;for(std::size_t i=0;i<a.size();++i){near(a[i].geometry.point_m,b[i].geometry.point_m);near(a[i].geometry.normal,b[i].geometry.normal);near(a[i].velocity_m_s,b[i].velocity_m_s);ContactHistory ah,bh;auto aw=evaluate_moving_wall(p,a[i],model,ah,1e-7),bw=evaluate_moving_wall(p,b[i],model,bh,1e-7);near(aw.particle.force_a,bw.particle.force_a);near(aw.particle.torque_a,bw.particle.torque_a);near(aw.wall.reaction_force_n,bw.wall.reaction_force_n);near(aw.wall.reaction_torque_nm,bw.wall.reaction_torque_nm);near(aw.wall.power_to_particles_w,bw.wall.power_to_particles_w);}}
 moving.guard_prepared(p.position_m,p.position_m,pose,end);
 rejects([&]{moving.guard_prepared(m.point({0,0,.1},t),m.point({0,0,-.1},t+1e-7),pose,end);});
 }
 auto a=clump_demo();a.bodies.resize(1);a.boxes.clear();a.bodies[0].position_m={.1,.12,.069999};a.bodies[0].velocity_m_s={.02,0,0};PrescribedMotion motion;motion.rpm=5;motion.linear_velocity_m_s.z=.001;a.moving_meshes.emplace_back(mesh,motion);auto b=a;b.use_prepared_walls=false;ClumpProfile ap,bp;
 for(int i=0;i<4000;++i){auto ar=a.step_profiled(1e-7,&ap),br=b.step_profiled(1e-7,&bp);check(ar.wall_contacts==br.wall_contacts&&a.histories()==b.histories(),"world wall parity");near(a.bodies[0].position_m,b.bodies[0].position_m);near(a.bodies[0].velocity_m_s,b.bodies[0].velocity_m_s);near(a.bodies[0].angular_momentum_world,b.bodies[0].angular_momentum_world);near(a.moving_wall_work_j,b.moving_wall_work_j);near(ar.moving_wall_loads[0].reaction_torque_nm,br.moving_wall_loads[0].reaction_torque_nm);}
 check(ap.wall_pose_builds==8000&&ap.wall_analytic_queries>0&&ap.wall_triangle_checks<bp.wall_triangle_checks,"shared wall accounting");const auto before=serialize_clump_state(a);rejects([&]{a.step(1);});check(before==serialize_clump_state(a),"wall failure atomicity");
 std::cout<<"PASS certified faces, seams/edges/corners, sheets/ports/thin walls, negative/zero/positive RPM, force/torque/work and atomic guards\nanalytic_queries="<<ap.wall_analytic_queries<<" triangle_checks="<<ap.wall_triangle_checks<<" reference_checks="<<bp.wall_triangle_checks<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
