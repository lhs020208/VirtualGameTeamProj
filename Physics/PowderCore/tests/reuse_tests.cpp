#include "powder/clump.hpp"
#include <cstdlib>
#include <new>
#include <iostream>
#include <stdexcept>
#include <random>
#include <cmath>
static bool counting=false;
static std::size_t allocations=0,requested_bytes=0;
void* operator new(std::size_t n){if(counting){++allocations;requested_bytes+=n;}if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
using namespace powder;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
std::vector<NodePair> touching(const std::vector<Particle>& p,const std::vector<NodePair>& pairs){std::vector<NodePair> result;for(auto ij:pairs){const auto& a=p[ij.first];const auto& b=p[ij.second];if((a.diameter_m+b.diameter_m)/2>std::hypot(a.position_m.x-b.position_m.x,a.position_m.y-b.position_m.y,a.position_m.z-b.position_m.z))result.push_back(ij);}return result;}
int main(){try{
 std::mt19937 rng(179);std::uniform_real_distribution<double> pos(-.2,.2),motion(-.0003,.0003);
 std::vector<Particle> p(120);std::vector<std::size_t> owner;for(std::size_t i=0;i<p.size();++i){p[i].id=i+1;p[i].diameter_m=i%3?.02:.01;p[i].position_m={pos(rng),pos(rng),pos(rng)};owner.push_back(i/2);}
 ClumpNeighbors cache;ClumpProfile profile;
 for(int t=0;t<300;++t){for(auto& a:p){a.position_m.x+=motion(rng);a.position_m.y+=motion(rng);a.position_m.z+=motion(rng);}if(t==50){p[0].diameter_m=.04;}if(t==80){p[1].id=900;}if(t==100){p.pop_back();owner.pop_back();}if(t==130){p.push_back(p.front());p.back().id=999;owner.push_back(1000);}if(t==160)owner[1]=300;
 auto exact=touching(p,clump_candidates(p,owner,false));check(touching(p,cache.query(p,owner,true,true,&profile))==exact,"moving/edited neighbor omission");}
 check(profile.neighbor_rebuilds>1,"missing displacement invalidation");
 // Reuse with no motion, topology edits and explicit invalidation.
 cache.query(p,owner,true,true,&profile);auto before=profile.neighbor_rebuilds;cache.query(p,owner,true,true,&profile);check(profile.neighbor_rebuilds==before,"stationary cache not reused");cache.invalidate();cache.query(p,owner,true,true,&profile);check(profile.neighbor_rebuilds==before+1,"explicit invalidation");
 // Rotation moves pebbles even when COM stays fixed.
 auto body=make_clump(1,0,{{{-.02,0,0},.04},{{.02,0,0},.04}},2500);auto rotor=clump_particles(body);ClumpNeighbors rotating;ClumpProfile rp;rotating.query(rotor,{0,0},true,true,&rp);body.orientation={std::cos(.25),0,0,std::sin(.25)};rotor=clump_particles(body);rotating.query(rotor,{0,0},true,true,&rp);check(rp.neighbor_rebuilds==2,"rotation displacement ignored");
 // A pair initially outside skin must be found after a fast approach.
 std::vector<Particle> fast(2);fast[0].id=1;fast[1].id=2;for(auto& a:fast)a.diameter_m=.02;fast[1].position_m.x=.03;ClumpNeighbors approaching;ClumpProfile fp;check(approaching.query(fast,{0,1},true,true,&fp).empty(),"fast fixture");fast[1].position_m.x=.019;check(touching(fast,approaching.query(fast,{0,1},true,true,&fp)).size()==1&&fp.neighbor_rebuilds==2,"fast approach missed");
 auto a=clump_demo();for(auto& b:a.bodies)b.position_m.z=.019999;auto b=a;b.reuse_neighbor_list=false;ClumpProfile ap,bp;
 for(int i=0;i<3000;++i){auto ar=a.step_profiled(1e-7,&ap),br=b.step_profiled(1e-7,&bp);check(ar.pair_contacts==br.pair_contacts&&ar.wall_contacts==br.wall_contacts&&ar.max_overlap_m==br.max_overlap_m,"contact/force report mismatch");check(serialize_clump_state(a)==serialize_clump_state(b)&&a.histories()==b.histories(),"reuse state/history mismatch");}
 check(ap.neighbor_reuses>ap.neighbor_rebuilds&&bp.neighbor_rebuilds==6000,"reuse accounting");
 const auto old=serialize_clump_state(a);bool rejected=false;try{a.step(1);}catch(const std::invalid_argument&){rejected=true;}check(rejected&&old==serialize_clump_state(a),"rollback");a.step(1e-7);b.step(1e-7);check(serialize_clump_state(a)==serialize_clump_state(b),"failed cache poisoned next step");a.reset_history();b.reset_history();a.step(1e-7);b.step(1e-7);check(serialize_clump_state(a)==serialize_clump_state(b),"reset histories");
 // Count actual global C++ heap allocations only inside warmed step calls.
 auto sample=[](ClumpWorld w,bool reuse){w.reuse_neighbor_list=reuse;for(int i=0;i<5;++i)w.step(1e-7);allocations=requested_bytes=0;counting=true;for(int i=0;i<200;++i)w.step(1e-7);counting=false;std::cout<<(reuse?"reuse":"rebuild")<<"_allocations="<<allocations<<" requested_bytes="<<requested_bytes<<" workspace_capacity_bytes="<<w.workspace_capacity_bytes()<<'\n';return allocations;};
 auto no=sample(b,false),yes=sample(b,true);check(yes<no,"allocations not reduced");
 std::cout<<"neighbor_rebuilds="<<ap.neighbor_rebuilds<<" neighbor_reuses="<<ap.neighbor_reuses<<" neighbor_capacity_bytes="<<ap.neighbor_capacity_bytes<<'\n';
 std::cout<<"PASS exact reuse/rebuild trajectories, contacts, topology/diameter/rotation/fast approach, rollback, history reset, allocation reduction\n";return 0;
 }catch(const std::exception& e){counting=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
