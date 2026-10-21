#include "powder/world.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace powder;
void check(bool x,const char* s){if(!x)throw std::runtime_error(s);}
void near(double x,double y,double tol,const char* s){check(std::abs(x-y)<=tol,s);}
Particle particle(Vec3 x,double d=1){Particle p;p.id=1;p.position_m=x;p.diameter_m=d;p.material.density_kg_m3=6/(3.14159265358979323846*d*d*d);return p;}
FixedBox box(std::uint64_t id,Vec3 x,Vec3 h,std::uint64_t group=1){FixedBox b;b.id=id;b.centre_m=x;b.half_m=h;b.weld_group=group;return b;}
template<class F>void rejects(F f,const char* name){bool ok=false;try{f();}catch(const std::invalid_argument&){ok=true;}check(ok,name);}
int main(int argc,char** argv){try{
 auto b=box(1,{0,0,0},{1,1,1});
 for(int feature=1;feature<=3;++feature){auto p=particle({1.2,feature>=2?1.2:0,feature>=3?1.2:0});auto c=box_contacts(p,{b});check(c.size()==1,"one contact per convex box");check(c[0].feature==feature,"face edge corner classification");near(c[0].overlap_m,.5-.2*std::sqrt(double(feature)),1e-12,"analytic overlap");near(c[0].normal.x,1/std::sqrt(double(feature)),1e-12,"analytic normal");}
 check(box_contacts(particle({1.6,0,0}),{b}).empty(),"finite box no contact outside radius");
 rejects([&]{box_contacts(particle({0,0,0}),{b});},"interior rejected");
 auto rotated=b;rotated.centre_m={4,5,6};const double c=std::sqrt(.5);rotated.axes={Vec3{c,c,0},Vec3{-c,c,0},Vec3{0,0,1}};rotated.half_m={2,1,.5};
 auto hit=box_contacts(particle({4+2.2*c,5+2.2*c,6}),{rotated});check(hit.size()==1,"rotated scaled contact");near(hit[0].overlap_m,.3,1e-12,"rotation invariant overlap");near(hit[0].normal.y,c,1e-12,"rotated normal");
 rotated.axes[0]={-c,-c,0};near(box_contacts(particle({4+2.2*c,5+2.2*c,6}),{rotated})[0].overlap_m,.3,1e-12,"negative scale reflection");
 auto bad=b;bad.axes[1]={1,0,0};rejects([&]{validate_boxes({bad});},"shear rejected");rejects([&]{validate_boxes({b,b});},"duplicate ids rejected");
 std::istringstream broken("POWDER_BOXES_SI 1 1 1");rejects([&]{read_boxes(broken);},"truncated geometry rejected");
 // Two adjacent flat tiles must behave like ONE plane through the seam.
 std::vector<FixedBox> tiles{box(11,{-1,0,-.5},{1,2,.5}),box(12,{1,0,-.5},{1,2,.5})};
 std::uint64_t key=0;
 for(int i=0;i<=200;++i){auto hits=box_contacts(particle({-.2+.002*i,0,.49}),tiles);check(hits.size()==1,"no double seam forces");near(hits[0].normal.x,0,1e-12,"no artificial seam sideways normal");near(hits[0].overlap_m,.01,1e-12,"seam no missed overlap");if(i==0)key=hits[0].surface_id;check(hits[0].surface_id==key,"weld history stable across seam");}
 auto overlap_tiles=tiles;overlap_tiles[0].half_m.x=1.1;overlap_tiles[1].half_m.x=1.1;
 check(box_contacts(particle({0,0,.49}),overlap_tiles).size()==1,"overlapping coplanar tiles deduplicated");
 // Real perpendicular walls remain separate constraints.
 auto corner=box_contacts(particle({.49,.49,0}),{box(21,{-.5,0,0},{.5,2,2}),box(22,{0,-.5,0},{2,.5,2})});check(corner.size()==2,"real concave corner preserved");
 ParticleSystem tiled,plane;tiled.model.law=ContactLaw::Linear;plane.model.law=ContactLaw::Linear;tiled.boxes=tiles;plane.planes={{1,{0,0,1},0}};
 tiled.particles={particle({-.15,0,.499})};tiled.particles[0].velocity_m_s.x=1;plane.particles=tiled.particles;
 for(int i=0;i<10000;++i){tiled.step(2e-5);plane.step(2e-5);}
 check(tiled.particles[0].position_m.x>0,"dynamic seam crossed");
 for(auto member:{&Vec3::x,&Vec3::y,&Vec3::z}){near(tiled.particles[0].position_m.*member,plane.particles[0].position_m.*member,1e-10,"seam plane trajectory");near(tiled.particles[0].velocity_m_s.*member,plane.particles[0].velocity_m_s.*member,1e-10,"seam plane velocity");near(tiled.particles[0].angular_velocity_rad_s.*member,plane.particles[0].angular_velocity_rad_s.*member,1e-10,"seam plane spin");}
 ParticleSystem hz;hz.model.law=ContactLaw::HertzMindlin;hz.gravity_m_s2={};hz.boxes={b};hz.particles={particle({1.4999,0,0})};auto hr=hz.step(1e-7);check(hr.wall_contacts==1&&hz.particles[0].velocity_m_s.x>0,"Hertz box force");
 auto before=hz.particles[0];rejects([&]{hz.step(.1);},"box invalid dt");near(hz.particles[0].position_m.x,before.position_m.x,0,"box failure atomic");
 ParticleSystem edge;edge.gravity_m_s2={};edge.model.law=ContactLaw::Linear;edge.model.linear.mu=0;edge.boxes={b};edge.particles={particle({1.2,1.2,0})};edge.step(1e-5);
 near(edge.particles[0].velocity_m_s.x,10000*(.5-std::sqrt(.08))/std::sqrt(2.)*1e-5,1e-12,"edge force analytic");near(edge.particles[0].velocity_m_s.x,edge.particles[0].velocity_m_s.y,1e-12,"edge force symmetry");
 check(argc==2,"fixture argument");std::ifstream in(argv[1]);check(bool(in),"open Unreal fixture");auto rig=read_boxes(in);check(rig.size()==53,"53 actual Unreal boxes loaded");
 // Central open path through feeder, funnel and tube, including both tube ends.
 for(int i=0;i<=180;++i){double z=2.2+i*.02;check(box_contacts(particle({0,0,z},.02),rig).empty(),"central inlet/outlet stays open");}
 // Sweep inside tube near its polygonal inner surface: geometric contacts must exist.
 for(int i=0;i<360;++i){double angle=i*3.14159265358979323846/180;auto hits=box_contacts(particle({.119*std::cos(angle),.119*std::sin(angle),3},.02),rig);check(!hits.empty(),"tube wall seam coverage");check(hits.size()<=2,"tube seam bounded manifold");}
 ParticleSystem fall;fall.model.law=ContactLaw::Linear;fall.boxes=rig;fall.particles={particle({0,0,5.5},.2)};
 fall.model.linear={4e6,800,1e6,200,.3};
 bool passed_tube=false,collector=false;double max_overlap=0;
 for(int i=0;i<750000;++i){auto r=fall.step(2e-6);if(fall.particles[0].position_m.z<2.3)passed_tube=true;if(r.wall_contacts)collector=true;max_overlap=std::max(max_overlap,r.max_overlap_m);}
 check(passed_tube&&collector,"actual fixture open passage and collector contact");check(fall.particles[0].position_m.z>1.04,"collector not penetrated");
 check(max_overlap<.005,"rig overlap below five percent of radius");
 std::cout<<"PASS box faces/edges/corners, transforms, welded seam friction, Hertz and actual 53-box rig; final z="<<fall.particles[0].position_m.z<<" maxOverlap="<<max_overlap<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
