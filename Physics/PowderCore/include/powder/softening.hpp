#pragma once
#include "powder/elastic.hpp"
#include <cmath>
#include <stdexcept>
namespace powder {
enum class SofteningScope { BulkWideFlow, ClearanceOrClogging, ImpactOrDeformation };
struct SofteningModel {
 MaterialContact physical;
 MaterialContact computational;
 double stiffness_ratio=1;
 double damping_ratio=1;
 SofteningScope scope=SofteningScope::BulkWideFlow;
};
// Experimental linear bulk surrogate, not an alteration of measured material data.
// At fixed mass, c/sqrt(k*m) is preserved; this is NOT a restitution guarantee
// for frictional, clipped or multi-contact motion. No geometry/overlap guard changes.
inline SofteningModel soften_linear_bulk(const MaterialContact& physical,double ratio,SofteningScope scope){
 validate_contact_model(physical);
 if(!std::isfinite(ratio)||ratio<=0||ratio>1)throw std::invalid_argument("softening ratio must be finite in (0,1]");
 if(scope!=SofteningScope::BulkWideFlow&&scope!=SofteningScope::ClearanceOrClogging&&scope!=SofteningScope::ImpactOrDeformation)throw std::invalid_argument("unknown softening scope");
 SofteningModel result{physical,physical,ratio,1,scope};
 if(ratio==1)return result;
 if(scope!=SofteningScope::BulkWideFlow)throw std::invalid_argument("softening forbidden for clearance/clogging or impact/deformation");
 if(physical.law!=ContactLaw::Linear)throw std::invalid_argument("Hertz softening is not validated");
 if(physical.rolling.mu!=0||physical.adhesion_gamma_j_m2!=0)throw std::invalid_argument("rolling/adhesive softening is not validated");
 result.damping_ratio=std::sqrt(ratio);
 result.computational.linear.kn=physical.linear.kn*ratio;
 result.computational.linear.kt=physical.linear.kt*ratio;
 result.computational.linear.cn=physical.linear.cn*result.damping_ratio;
 result.computational.linear.ct=physical.linear.ct*result.damping_ratio;
 validate_contact_model(result.computational);return result;
}
}
