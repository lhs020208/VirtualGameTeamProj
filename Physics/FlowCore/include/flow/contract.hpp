#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace flow {
inline constexpr const char* Version="0.2.0";
inline constexpr double GasConstant=287.05, Gamma=1.4, TemperatureK=293.15, ViscosityPaS=1.81e-5;
enum class BoundaryKind { NoSlip, PressureAbsolute, ParabolicVelocity };
// value: 0 for NoSlip, Pa(abs) for PressureAbsolute, mean m/s for ParabolicVelocity.
struct Boundary { BoundaryKind kind=BoundaryKind::NoSlip; double value=0; };
enum class InitialCondition { Rest, SteadyAnalyticReference };
struct Grid {
    int nx=64,ny=16;
    double length_m=.1,height_m=.01,reporting_depth_m=1;
};
struct ProjectionCriteria {
    int max_iterations=20000;
    double residual_absolute_pa_m2=1e-7,residual_relative=1e-8;
    double divergence_linf_s_inv=1e-7,mass_balance_relative=1e-8;
};
struct Contract {
    Grid grid;
    double reference_pressure_absolute_pa=101325,temperature_k=TemperatureK;
    // Left/right/bottom/top; only the documented open channel topology is supported.
    std::array<Boundary,4> boundaries{{{BoundaryKind::PressureAbsolute,101325.004344},
                                     {BoundaryKind::PressureAbsolute,101325},{},{}}};
    double dt_s=.0005,duration_s=5,velocity_component_bound_m_s=.05;
    ProjectionCriteria projection;
    InitialCondition initial=InitialCondition::Rest;
};
struct Derived {
    double rho_kg_m3=0,nu_m2_s=0,sound_speed_m_s=0;
    double dx_m=0,dy_m=0,hydraulic_diameter_m=0;
    double estimated_pressure_drop_pa=0,estimated_inlet_absolute_pa=0;
    double analytic_mean_m_s=0,analytic_peak_m_s=0,analytic_mass_flow_kg_s=0;
    double reynolds_bound=0,mach_bound=0,knudsen_estimate=0,relative_pressure_span=0;
    double advective_number=0,diffusive_number=0,combined_number=0,max_dt_s=0;
    std::uint64_t steps=0;
};
double absolute_from_gauge(double gauge_pa,double ambient_absolute_pa);
double gauge_from_absolute(double absolute_pa,double ambient_absolute_pa);
Derived validate(const Contract& c);
Contract parse_contract(const std::string& text);
std::string serialize_contract(const Contract& c);
std::string contract_report_json(const Contract& c);
// Independent steady reference, not an evolved CFD solution.
double analytic_velocity(const Contract& c,double y_m);
double inlet_face_velocity(const Contract& c,int j);
double analytic_pressure_gauge(const Contract& c,double x_m);

// Interior pressure cells and physical velocity faces, without ghost storage.
class Layout {
public:
    explicit Layout(const Contract& c);
    std::size_t pressure_count()const;
    std::size_t u_count()const;
    std::size_t v_count()const;
    std::size_t p_index(int i,int j)const;
    std::size_t u_index(int i,int j)const;
    std::size_t v_index(int i,int j)const;
    std::array<double,2> p_position(int i,int j)const;
    std::array<double,2> u_position(int i,int j)const;
    std::array<double,2> v_position(int i,int j)const;
private:
    int nx_,ny_;double dx_,dy_;
};
struct Fields {
    std::vector<double> pressure_gauge_pa,u_m_s,v_m_s;
    std::uint64_t accepted_steps=0;
    double physical_time_s=0;
};
Fields make_initial_fields(const Contract& c);
void validate_fields(const Contract& c,const Fields& f);
// Time integration is in solver.hpp. Initial data do not imply convergence.
}
