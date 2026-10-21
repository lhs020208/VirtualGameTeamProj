#include "powder/clump.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){try{
auto a=powder::clump_demo(),b=a;powder::ClumpProfile profile;a.use_spatial_search=b.use_spatial_search=false;
for(int i=0;i<100;++i){auto ar=a.step(2e-6),br=b.step_profiled(2e-6,&profile);check(ar.pair_contacts==br.pair_contacts&&ar.wall_contacts==br.wall_contacts&&ar.mass_kg==br.mass_kg,"reports changed");}
check(powder::serialize_clump_state(a)==powder::serialize_clump_state(b)&&a.histories()==b.histories(),"instrumentation changed state");
check(profile.attempted_steps==100&&profile.accepted_steps==100&&profile.material_pair_validations==4&&profile.material_table_rebuilds==1&&profile.property_rebuilds==6&&profile.property_hits==594,"step/validation counters");check(profile.pair_distance_checks==3300&&profile.post_pair_checks==3300,"external-node pair counters");check(profile.total_seconds()>0,"timer empty");for(double t:profile.seconds)check(std::isfinite(t)&&t>=0,"invalid timing");
for(int i=0;i<100;++i){a.step(2e-6);b.step(2e-6);}check(powder::serialize_clump_state(a)==powder::serialize_clump_state(b),"continued history mismatch");
const auto old=powder::serialize_clump_state(b);const double clock=b.physical_time_s;bool rejected=false;try{b.step_profiled(1,&profile);}catch(const std::invalid_argument&){rejected=true;}
check(rejected&&old==powder::serialize_clump_state(b)&&clock==b.physical_time_s&&profile.attempted_steps==101&&profile.accepted_steps==100,"failed-step timing/atomicity");
auto large=powder::clump_demo();large.bodies.resize(301,large.bodies.front());rejected=false;try{large.step(2e-6);}catch(const std::invalid_argument& e){rejected=std::string(e.what())=="clump body limit 300";}check(rejected,"capacity contract changed");
std::cout<<"PASS opt-in timers/counters, identical physics and continued history, failure accounting, unchanged capacity\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
