#pragma once
#include "flow/contract.hpp"

namespace flow {
inline constexpr const char* SolverVersion = "0.2.0";
struct PressureResult {
    std::vector<double> pressure;
    int iterations=0;
    double residual_linf_pa_m2=0;
};
// Solves -laplacian(p)=source, with face Dirichlet x boundaries (left may
// instead be homogeneous Neumann), and homogeneous Neumann y boundaries.
// Boundary contributions are added internally, not part of source.
PressureResult solve_pressure(const Contract& c,const std::vector<double>& source,
    const std::vector<double>& left,const std::vector<double>& right,
    const std::vector<double>& guess,double residual_cap=1e100);
struct StepReport {
    int pressure_iterations=0;
    double pressure_residual_pa_m2=0,divergence_linf_s_inv=0;
    double inlet_mass_kg_s=0,outlet_mass_kg_s=0,mass_balance_relative=0;
    double max_speed_m_s=0,kinetic_energy_j=0;
    std::array<double,2> applied_source_impulse_n_s{{0,0}};
};
// Transactional: on any failure f remains unchanged, including its clock.
StepReport advance(const Contract& c,Fields& f);
// Force in N per interior MAC control volume, not force density.
// Boundary-face forces must be zero. The unforced API is unchanged.
struct MomentumSource { std::vector<double> u_force_n,v_force_n; };
StepReport advance_with_source(const Contract& c,Fields& f,const MomentumSource& source);
StepReport diagnostics(const Contract& c,const Fields& f);
}
