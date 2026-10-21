#include "powder/clump.hpp"
#include "powder/spatial.hpp"
#include <random>
#include <set>
#include <iostream>
#include <stdexcept>
using namespace powder;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
std::vector<NodePair> contacts(const std::vector<Particle>& p,const std::vector<std::size_t>& owners,bool grid){std::vector<NodePair> out;auto candidates=clump_candidates(p,owners,grid);check(std::is_sorted(candidates.begin(),candidates.end()),"unordered candidates");check(std::adjacent_find(candidates.begin(),candidates.end())==candidates.end(),"duplicate candidates");for(auto ij:candidates){const auto& a=p[ij.first];const auto& b=p[ij.second];check(owners[ij.first]!=owners[ij.second],"internal pair");if((a.diameter_m+b.diameter_m)/2>std::hypot(a.position_m.x-b.position_m.x,a.position_m.y-b.position_m.y,a.position_m.z-b.position_m.z))out.push_back(ij);}return out;}
int main(){try{
 std::mt19937 rng(314159);std::uniform_real_distribution<double> pos(-.15,.15),diam(.001,.04);
 for(int trial=0;trial<100;++trial){std::vector<Particle> p;std::vector<std::size_t> owners;for(int i=0;i<128;++i){Particle a;a.id=i+1;a.diameter_m=diam(rng);a.position_m={pos(rng),pos(rng),pos(rng)};p.push_back(a);owners.push_back(i/2);}check(contacts(p,owners,true)==contacts(p,owners,false),"polydisperse contact omission");p.erase(p.begin());owners.erase(owners.begin());check(contacts(p,owners,true)==contacts(p,owners,false),"deletion mismatch");}
 for(double offset:{-1e13,-.0404,-.0202,0.,.0202,.0404,1e13}){std::vector<Particle> p(4);std::vector<std::size_t> owners{0,1,1,2};for(std::size_t i=0;i<p.size();++i){p[i].id=i+1;p[i].diameter_m=.02;p[i].position_m={offset+(double(i)-1)*.019999,0,0};}check(contacts(p,owners,true)==contacts(p,owners,false),"boundary/fallback mismatch");}
 // Sparse hash fallback, extreme size ratio and degenerate occupancy.
 {std::vector<Particle> p(80);std::vector<std::size_t> owners;for(std::size_t i=0;i<p.size();++i){p[i].id=i+1;p[i].diameter_m=i%2?.00002:.02;p[i].position_m={double(i)*100,0,0};owners.push_back(i);}p[1].position_m.x=.009999;check(contacts(p,owners,true)==contacts(p,owners,false),"sparse hash omission");check(!contacts(p,owners,true).empty(),"sparse probe lacks contact");for(auto& n:p)n.position_m={};check(contacts(p,owners,true)==contacts(p,owners,false),"single-cell degeneracy");}
 for(int shape=0;shape<2;++shape){auto a=clump_demo();if(!shape){for(auto& b:a.bodies){b.pebbles={{{},.04}};b.position_m.z=.019999;}}auto b=a;b.use_spatial_search=false;ClumpProfile ap,bp;
 for(int k=0;k<3000;++k){if(k==100){std::reverse(a.bodies.begin(),a.bodies.end());std::reverse(b.bodies.begin(),b.bodies.end());}if(k==200){a.bodies.pop_back();b.bodies.pop_back();}if(k==300){auto n=a.bodies.back();n.id=50;n.position_m.x=1;a.bodies.push_back(n);b.bodies.push_back(n);}auto ar=a.step_profiled(1e-7,&ap),br=b.step_profiled(1e-7,&bp);check(ar.pair_contacts==br.pair_contacts&&ar.wall_contacts==br.wall_contacts&&ar.max_overlap_m==br.max_overlap_m,"report mismatch");check(serialize_clump_state(a)==serialize_clump_state(b)&&a.histories()==b.histories(),"state/history mismatch");}check(ap.pair_distance_checks<bp.pair_distance_checks&&ap.post_pair_checks<bp.post_pair_checks,"no pruning");}
 // Pair is separated initially but crosses the Hertz cap at the end of this step.
 auto a=clump_demo();a.bodies.resize(2);a.boxes.clear();a.gravity_m_s2={};MaterialContact m;m.max_overlap_radius_ratio=.00001;a.wall_models={{0,m}};a.pair_models={{{0,0},m}};
 for(int i=0;i<2;++i){a.bodies[i]=make_clump(i+1,0,{{{},.04}},2500);a.bodies[i].position_m={i*.0400001,0,.2};a.bodies[i].velocity_m_s={i?-1.:1.,0,0};}
 auto b=a;b.use_spatial_search=false;for(auto* w:{&a,&b}){const auto old=serialize_clump_state(*w);bool rejected=false;try{w->step(1e-6);}catch(const std::invalid_argument& e){rejected=std::string(e.what())=="clump Hertz pair overlap cap";}check(rejected&&old==serialize_clump_state(*w)&&w->physical_time_s==0&&w->histories()==0,"new post-step contact or rollback missed");}
 std::cout<<"PASS grid/exhaustive sets, polydispersity, signed cell boundaries, extreme fallback, owner exclusion, edits, exact trajectories and new post-step contact rejection\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
