#include "powder/softening.hpp"
#include "powder/clump.hpp"
#include <iostream>
using namespace powder;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"missing policy rejection");}
int main(){try{
 auto world=clump_demo();auto p=world.wall_models.at(0);const auto original=p;
 for(double ratio:{1.,.5,.25,.1}){auto s=soften_linear_bulk(p,ratio,SofteningScope::BulkWideFlow);
  check(s.physical.linear.kn==original.linear.kn&&p.linear.cn==original.linear.cn,"physical model mutated");
  check(s.computational.linear.kn==p.linear.kn*ratio&&s.computational.linear.kt==p.linear.kt*ratio,"stiffness scaling");
  check(std::abs(s.computational.linear.cn/std::sqrt(s.computational.linear.kn)-p.linear.cn/std::sqrt(p.linear.kn))<1e-14,"normal damping ratio");
  check(std::abs(s.computational.linear.ct/std::sqrt(s.computational.linear.kt)-p.linear.ct/std::sqrt(p.linear.kt))<1e-14,"tangent damping ratio");
  check(s.computational.linear.mu==p.linear.mu&&s.computational.max_overlap_radius_ratio==p.max_overlap_radius_ratio&&s.computational.a.young_pa==p.a.young_pa,"unrelated physical data changed");
 }
 for(auto scope:{SofteningScope::ClearanceOrClogging,SofteningScope::ImpactOrDeformation}){rejects([&]{soften_linear_bulk(p,.5,scope);});check(soften_linear_bulk(p,1,scope).computational.linear.kn==p.linear.kn,"identity prohibited");}
 for(double r:{0.,-1.,1.01,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})rejects([&]{soften_linear_bulk(p,r,SofteningScope::BulkWideFlow);});
 auto h=p;h.law=ContactLaw::HertzMindlin;rejects([&]{soften_linear_bulk(h,.5,SofteningScope::BulkWideFlow);});
 auto a=p;a.adhesion_gamma_j_m2=.001;rejects([&]{soften_linear_bulk(a,.5,SofteningScope::BulkWideFlow);});a=p;a.rolling.mu=.1;rejects([&]{soften_linear_bulk(a,.5,SofteningScope::BulkWideFlow);});
 auto same=world;for(auto& e:same.pair_models)e.second=soften_linear_bulk(e.second,1,SofteningScope::BulkWideFlow).computational;for(auto& e:same.wall_models)e.second=soften_linear_bulk(e.second,1,SofteningScope::BulkWideFlow).computational;
 for(int i=0;i<1000;++i){world.step(2e-6);same.step(2e-6);}check(serialize_clump_state(world)==serialize_clump_state(same),"identity physics differs");
 std::cout<<"PASS physical/computational separation, damping invariant, identity trajectory and scope/model rejection\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
