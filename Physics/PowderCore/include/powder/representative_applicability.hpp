#pragma once
#include "powder/representative.hpp"
namespace powder {
struct RepresentativeClearance {
 double physical_diameter_m=0,computed_diameter_m=0,opening_m=0;
 bool physical_fits=false,computed_fits=false,changes_passability=false;
};
// Rigid sphere versus straight aperture geometry only; not an arching prediction.
inline RepresentativeClearance representative_clearance(double diameter,double opening,std::uint64_t q){
 if(!std::isfinite(diameter)||diameter<=0||!std::isfinite(opening)||opening<=0)throw std::invalid_argument("positive finite clearance dimensions required");
 const double computed=diameter*representative_length_scale(q);
 if(!std::isfinite(computed))throw std::invalid_argument("computed diameter overflow");
 // Equality is deliberately not certified: a zero-clearance fit is not robust.
 return {diameter,computed,opening,diameter<opening,computed<opening,(diameter<opening)!=(computed<opening)};
}
}
