#include "powder/representative_applicability.hpp"
#include "powder/representative_runtime.hpp"
#include <iostream>
using namespace powder;
void check(bool b){if(!b)throw std::runtime_error("applicability contract failed");}
int main(){try{
 MaterialContact m;m.law=ContactLaw::Linear;unsigned rejects=0;
 for(unsigned q:{1,2,4,8}){
  auto small=representative_clearance(.01,.012,q),large=representative_clearance(.014,.012,q);check(small.physical_fits&&!large.physical_fits);check(small.computed_fits==(q==1));check(small.changes_passability==(q>1));check(!large.computed_fits&&!large.changes_passability);
  for(auto scope:{RepresentativeScope::ClearanceOrClogging,RepresentativeScope::SizeSegregation}){
   bool rejected=false;try{representative_world({{0,0,.01,2500,q}},q,{{1,0,0,{0,0,.02},{}}},{{{0,0},m}},{{0,m}},scope);}catch(const std::invalid_argument&){rejected=true;}
   check(rejected==(q>1));if(rejected)++rejects;
  }
 }
 check(rejects==6);check(!representative_clearance(.01,.01,1).computed_fits);
 unsigned invalid=0;for(double d:{0.,-1.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})try{representative_clearance(d,.012,1);}catch(const std::invalid_argument&){++invalid;}check(invalid==4);
 std::cout<<"representative applicability passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
