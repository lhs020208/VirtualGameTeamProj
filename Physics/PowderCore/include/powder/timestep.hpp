#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
namespace powder {
// Diagnostics for the EXISTING rigid-clump bound. Not a convergence certificate.
struct StepScale {
 double minimum_radius_m=0, minimum_body_mass_kg=0, maximum_mobility=0;
 double maximum_stiffness_n_m=0, maximum_damping_ns_m=0;
 std::size_t maximum_contacts=0;
 double elastic_rate_s=0, damping_rate_s=0;
 double contact_limit_s=std::numeric_limits<double>::infinity();
 double moving_wall_limit_s=std::numeric_limits<double>::infinity();
};
inline StepScale rigid_step_scale(double mobility,double stiffness,double damping,std::size_t contacts){
 if(!std::isfinite(mobility)||mobility<=0||!std::isfinite(stiffness)||stiffness<=0||!std::isfinite(damping)||damping<0)throw std::invalid_argument("invalid rigid step scale");
 StepScale s;s.maximum_mobility=mobility;s.maximum_stiffness_n_m=stiffness;s.maximum_damping_ns_m=damping;s.maximum_contacts=contacts;
 const double count=static_cast<double>(std::max<std::size_t>(1,contacts));
 s.elastic_rate_s=std::sqrt(2*count*stiffness*mobility);s.damping_rate_s=2*count*damping*mobility;
 const double rate=s.elastic_rate_s+s.damping_rate_s;
 if(!std::isfinite(rate)||rate<=0)throw std::invalid_argument("nonfinite rigid step scale");
 s.contact_limit_s=.02/rate;return s;
}
}
