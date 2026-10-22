#include "powder/representative_runtime.hpp"
#include <iostream>
using namespace powder;
void check(bool b){if(!b)throw std::runtime_error("runtime contract failure");}
int main(){try{MaterialContact m;m.law=ContactLaw::Linear;m.linear={1000,.1,250,.025,.4};const std::vector<PhysicalSizeBin> bins{{0,0,.01,2500,8}};const std::map<std::pair<unsigned,unsigned>,MaterialContact> pairs{{{0,0},m}};const std::map<unsigned,MaterialContact> walls{{0,m}};
 std::vector<RepresentativePlacement> p;ClumpWorld reference;reference.pair_models=pairs;reference.wall_models=walls;
 for(unsigned i=0;i<8;++i){p.push_back({i+1,0,0,{i*.02,0,.1},{}});auto b=make_clump(i+1,0,{{{},.01}},2500);b.position_m=p.back().position_m;reference.bodies.push_back(b);}
 auto one=representative_world(bins,1,p,pairs,walls);for(int i=0;i<100;++i){one.step(.000002);reference.step(.000002);}check(serialize_clump_state(one)==serialize_clump_state(reference));
 auto coarse=representative_world(bins,8,{p.front()},pairs,walls);check(coarse.bodies.size()==1);check(std::abs(clump_properties(coarse.bodies[0].pebbles,2500).mass_kg-representative_inventory(bins,8).total_mass_kg)<1e-14);
 int rejected=0;auto reject=[&](auto f){try{f();}catch(const std::invalid_argument&){++rejected;}};
 reject([&]{representative_world(bins,8,p,pairs,walls);});auto duplicate=p;duplicate[1].id=duplicate[0].id;reject([&]{representative_world(bins,1,duplicate,pairs,walls);});reject([&]{representative_world(bins,8,{p.front()},{},walls);});reject([&]{representative_world(bins,8,{p.front()},pairs,walls,RepresentativeScope::ClearanceOrClogging);});auto bad=p;bad[0].bin=8;reject([&]{representative_world(bins,1,bad,pairs,walls);});reject([&]{representative_world(bins,8,{p.front()},pairs,{});});auto nan=p;nan[0].position_m.x=std::numeric_limits<double>::quiet_NaN();reject([&]{representative_world(bins,1,nan,pairs,walls);});auto missing=p;missing[0].group=99;reject([&]{representative_world(bins,1,missing,pairs,walls);});auto odd=bins;odd[0].physical_count=9;reject([&]{representative_world(odd,8,{p.front()},pairs,walls);});auto unsupported=pairs;unsupported.begin()->second.linear.adhesion_gamma_j_m2=1;reject([&]{representative_world(bins,8,{p.front()},unsupported,walls);});check(rejected==10);std::cout<<"representative runtime passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
