#include "flow/contract.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace flow {
namespace {
void require(bool ok,const char* reason){if(!ok)throw std::invalid_argument(reason);}
void finite(double x,const char* reason){require(std::isfinite(x),reason);}
void range(double x,double lo,double hi,const char* reason){require(std::isfinite(x)&&x>=lo&&x<=hi,reason);}
const char* kind_name(BoundaryKind k){switch(k){case BoundaryKind::NoSlip:return "no_slip";case BoundaryKind::PressureAbsolute:return "pressure_absolute_pa";case BoundaryKind::ParabolicVelocity:return "parabolic_mean_m_s";}throw std::invalid_argument("unknown boundary kind");}
const char* initial_name(InitialCondition i){switch(i){case InitialCondition::Rest:return "rest";case InitialCondition::SteadyAnalyticReference:return "steady_analytic_reference";}throw std::invalid_argument("unknown initial condition");}
void expect(std::istream& in,const char* wanted){std::string actual;require(bool(in>>actual)&&actual==wanted,"unexpected/missing contract field, units, model or schema");}
template<class T>T number(std::istream& in){T x{};require(bool(in>>x),"invalid numeric field");return x;}
Boundary boundary(std::istream& in){std::string name;require(bool(in>>name),"missing boundary kind");Boundary b;if(name=="no_slip")b.kind=BoundaryKind::NoSlip;else if(name=="pressure_absolute_pa")b.kind=BoundaryKind::PressureAbsolute;else if(name=="parabolic_mean_m_s")b.kind=BoundaryKind::ParabolicVelocity;else throw std::invalid_argument("unsupported boundary kind; gauge pressure must be explicitly converted");b.value=number<double>(in);return b;}
std::size_t index(int i,int j,int width,int height){require(i>=0&&i<width&&j>=0&&j<height,"MAC grid index outside storage");return static_cast<std::size_t>(i)+static_cast<std::size_t>(width)*static_cast<std::size_t>(j);}
}
double absolute_from_gauge(double gauge_pa,double ambient_absolute_pa){finite(gauge_pa,"nonfinite gauge pressure");require(std::isfinite(ambient_absolute_pa)&&ambient_absolute_pa>0,"ambient absolute pressure must be positive");const double result=gauge_pa+ambient_absolute_pa;require(std::isfinite(result)&&result>0,"converted absolute pressure must be positive");return result;}
double gauge_from_absolute(double absolute_pa,double ambient_absolute_pa){require(std::isfinite(absolute_pa)&&absolute_pa>0,"absolute pressure must be positive");require(std::isfinite(ambient_absolute_pa)&&ambient_absolute_pa>0,"ambient absolute pressure must be positive");const double result=absolute_pa-ambient_absolute_pa;finite(result,"pressure difference overflow");return result;}
Derived validate(const Contract& c){
    const auto& g=c.grid;
    require(g.nx>=4&&g.nx<=1024&&g.ny>=4&&g.ny<=512,"grid dimensions outside 4..1024 by 4..512");
    require(static_cast<std::uint64_t>(g.nx)*static_cast<std::uint64_t>(g.ny)<=262144,"pressure cell allocation limit 262144 exceeded");
    range(g.length_m,.002,1,"channel length outside 0.002..1 m");range(g.height_m,.001,.02,"channel gap outside 0.001..0.02 m; circular pipes are unsupported");
    range(g.length_m/g.height_m,2,50,"length/gap ratio outside 2..50");range(g.reporting_depth_m,.001,1,"reporting depth outside 0.001..1 m");
    range(c.reference_pressure_absolute_pa,95000,105000,"reference absolute pressure outside 95..105 kPa; strong vacuum requires a new model");
    require(std::isfinite(c.temperature_k)&&std::abs(c.temperature_k-TemperatureK)<=1e-9,"v1 supports isothermal 293.15 K air only");
    require(c.boundaries[0].kind==BoundaryKind::PressureAbsolute||c.boundaries[0].kind==BoundaryKind::ParabolicVelocity,"left face must be a pressure inlet or parabolic velocity inlet");
    require(c.boundaries[1].kind==BoundaryKind::PressureAbsolute,"right face must prescribe absolute outlet pressure (pressure anchor)");
    for(int i=2;i<4;++i)require(c.boundaries[i].kind==BoundaryKind::NoSlip&&c.boundaries[i].value==0,"top/bottom must be stationary no-slip walls with value 0");
    for(const auto& b:c.boundaries){kind_name(b.kind);finite(b.value,"nonfinite boundary value");if(b.kind==BoundaryKind::PressureAbsolute)range(b.value,95000,105000,"boundary absolute pressure outside 95..105 kPa; gauge values are not absolute pressure");}
    initial_name(c.initial);
    Derived d;d.rho_kg_m3=c.reference_pressure_absolute_pa/(GasConstant*c.temperature_k);d.nu_m2_s=ViscosityPaS/d.rho_kg_m3;d.sound_speed_m_s=std::sqrt(Gamma*GasConstant*c.temperature_k);
    d.dx_m=g.length_m/g.nx;d.dy_m=g.height_m/g.ny;d.hydraulic_diameter_m=2*g.height_m;range(d.dx_m/d.dy_m,.125,8,"cell aspect ratio outside 1/8..8");
    const double outlet=c.boundaries[1].value;
    if(c.boundaries[0].kind==BoundaryKind::PressureAbsolute){d.estimated_inlet_absolute_pa=c.boundaries[0].value;d.estimated_pressure_drop_pa=d.estimated_inlet_absolute_pa-outlet;require(d.estimated_pressure_drop_pa>=0,"reverse pressure flow is outside the v1 boundary contract");d.analytic_mean_m_s=d.estimated_pressure_drop_pa*g.height_m*g.height_m/(12*ViscosityPaS*g.length_m);}
    else {range(c.boundaries[0].value,0,1,"inlet mean speed outside 0..1 m/s");d.analytic_mean_m_s=c.boundaries[0].value;d.estimated_pressure_drop_pa=12*ViscosityPaS*g.length_m*d.analytic_mean_m_s/(g.height_m*g.height_m);d.estimated_inlet_absolute_pa=outlet+d.estimated_pressure_drop_pa;}
    range(d.estimated_inlet_absolute_pa,95000,105000,"estimated inlet absolute pressure outside supported range");
    d.relative_pressure_span=d.estimated_pressure_drop_pa/outlet;require(d.relative_pressure_span<=.01,"pressure span exceeds 1 percent: constant density assumption rejected");
    require(std::max(std::abs(outlet-c.reference_pressure_absolute_pa),std::abs(d.estimated_inlet_absolute_pa-c.reference_pressure_absolute_pa))/c.reference_pressure_absolute_pa<=.01,"boundary pressure differs from density reference by more than 1 percent");
    d.analytic_peak_m_s=1.5*d.analytic_mean_m_s;
    range(c.velocity_component_bound_m_s,1e-8,1,"declared component speed bound outside 1e-8..1 m/s");
    require(d.analytic_peak_m_s<=c.velocity_component_bound_m_s,"Poiseuille peak estimate exceeds declared speed bound");
    // Bound applies to each component; sqrt(2) bounds the 2D speed norm.
    const double speed=std::sqrt(2.)*c.velocity_component_bound_m_s;
    d.reynolds_bound=speed*d.hydraulic_diameter_m/d.nu_m2_s;require(d.reynolds_bound<=100,"Re bound exceeds conservative v1 laminar envelope 100");
    d.mach_bound=speed/d.sound_speed_m_s;require(d.mach_bound<=.1,"Mach bound exceeds 0.1");
    d.knudsen_estimate=70e-9*(101325/std::min(outlet,d.estimated_inlet_absolute_pa))/g.height_m;require(d.knudsen_estimate<=.001,"estimated Kn exceeds continuum envelope 0.001");
    range(c.dt_s,1e-9,1,"dt outside 1e-9..1 s");range(c.duration_s,c.dt_s,100,"duration must be at least dt and at most 100 s");
    d.advective_number=c.dt_s*c.velocity_component_bound_m_s*(1/d.dx_m+1/d.dy_m);
    d.diffusive_number=2*d.nu_m2_s*c.dt_s*(1/(d.dx_m*d.dx_m)+1/(d.dy_m*d.dy_m));d.combined_number=d.advective_number+d.diffusive_number;
    d.max_dt_s=.5/(d.combined_number/c.dt_s);require(d.combined_number<=.5,"combined explicit advection/diffusion budget exceeds 0.5; reduce dt");
    const double steps=c.duration_s/c.dt_s;require(std::isfinite(steps)&&steps<=1e7&&std::abs(steps-std::round(steps))<=1e-6,"duration/dt must be an integer with at most 10000000 steps");d.steps=static_cast<std::uint64_t>(std::llround(steps));
    const auto& p=c.projection;require(p.max_iterations>=1&&p.max_iterations<=1000000,"invalid pressure iteration limit");range(p.residual_absolute_pa_m2,1e-12,1e-2,"invalid absolute Poisson residual tolerance");range(p.residual_relative,1e-12,1e-3,"invalid relative Poisson residual tolerance");range(p.divergence_linf_s_inv,1e-12,1e-4,"invalid divergence tolerance");range(p.mass_balance_relative,1e-12,1e-4,"invalid mass balance tolerance");
    d.analytic_mass_flow_kg_s=d.rho_kg_m3*d.analytic_mean_m_s*g.height_m*g.reporting_depth_m;return d;
}
Contract parse_contract(const std::string& text){
    require(text.size()<=65536,"contract exceeds 64 KiB");std::istringstream in(text);in.imbue(std::locale::classic());Contract c;
    expect(in,"FLOW_CONTRACT");expect(in,"1");expect(in,"units");expect(in,"SI");expect(in,"model");expect(in,"planar_isothermal_laminar");expect(in,"geometry");expect(in,"parallel_plates");
    expect(in,"grid");c.grid.nx=number<int>(in);c.grid.ny=number<int>(in);c.grid.length_m=number<double>(in);c.grid.height_m=number<double>(in);
    expect(in,"reporting_depth_m");c.grid.reporting_depth_m=number<double>(in);expect(in,"reference_pressure_absolute_pa");c.reference_pressure_absolute_pa=number<double>(in);expect(in,"temperature_k");c.temperature_k=number<double>(in);
    const char* faces[]={"left","right","bottom","top"};for(int i=0;i<4;++i){expect(in,faces[i]);c.boundaries[i]=boundary(in);}
    expect(in,"time_s");c.dt_s=number<double>(in);c.duration_s=number<double>(in);expect(in,"velocity_component_bound_m_s");c.velocity_component_bound_m_s=number<double>(in);
    expect(in,"projection");c.projection.max_iterations=number<int>(in);c.projection.residual_absolute_pa_m2=number<double>(in);c.projection.residual_relative=number<double>(in);c.projection.divergence_linf_s_inv=number<double>(in);c.projection.mass_balance_relative=number<double>(in);
    expect(in,"initial");std::string initial;require(bool(in>>initial),"missing initial condition");if(initial=="rest")c.initial=InitialCondition::Rest;else if(initial=="steady_analytic_reference")c.initial=InitialCondition::SteadyAnalyticReference;else throw std::invalid_argument("unknown initial condition");
    expect(in,"end");std::string extra;require(!(in>>extra),"trailing/duplicate contract field");validate(c);return c;
}
std::string serialize_contract(const Contract& c){validate(c);std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17)<<"FLOW_CONTRACT 1\nunits SI\nmodel planar_isothermal_laminar\ngeometry parallel_plates\ngrid "<<c.grid.nx<<' '<<c.grid.ny<<' '<<c.grid.length_m<<' '<<c.grid.height_m<<"\nreporting_depth_m "<<c.grid.reporting_depth_m<<"\nreference_pressure_absolute_pa "<<c.reference_pressure_absolute_pa<<"\ntemperature_k "<<c.temperature_k<<'\n';const char* faces[]={"left","right","bottom","top"};for(int i=0;i<4;++i)o<<faces[i]<<' '<<kind_name(c.boundaries[i].kind)<<' '<<c.boundaries[i].value<<'\n';o<<"time_s "<<c.dt_s<<' '<<c.duration_s<<"\nvelocity_component_bound_m_s "<<c.velocity_component_bound_m_s<<"\nprojection "<<c.projection.max_iterations<<' '<<c.projection.residual_absolute_pa_m2<<' '<<c.projection.residual_relative<<' '<<c.projection.divergence_linf_s_inv<<' '<<c.projection.mass_balance_relative<<"\ninitial "<<initial_name(c.initial)<<"\nend\n";return o.str();}
std::string contract_report_json(const Contract& c){const auto d=validate(c);Layout l(c);std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17)<<"{\n\"schema\":1,\"flow_core_version\":\""<<Version<<"\",\"contract_valid\":true,\"solver_implemented\":true,\"flow_evolved\":false,\"result_kind\":\"input_contract_and_analytic_estimates_only\",\n\"model\":\"2D_planar_incompressible_isothermal_laminar\",\"units\":\"SI\",\"initial\":\""<<initial_name(c.initial)<<"\",\n";
    o<<"\"grid\":{\"nx\":"<<c.grid.nx<<",\"ny\":"<<c.grid.ny<<",\"length_m\":"<<c.grid.length_m<<",\"height_m\":"<<c.grid.height_m<<",\"reporting_depth_m\":"<<c.grid.reporting_depth_m<<",\"dx_m\":"<<d.dx_m<<",\"dy_m\":"<<d.dy_m<<",\"p_cells\":"<<l.pressure_count()<<",\"u_faces\":"<<l.u_count()<<",\"v_faces\":"<<l.v_count()<<"},\n\"boundaries\":[";
    for(int i=0;i<4;++i){if(i)o<<',';o<<"{\"kind\":\""<<kind_name(c.boundaries[i].kind)<<"\",\"value\":"<<c.boundaries[i].value<<'}';}o<<"],\n";
    o<<"\"reference_pressure_absolute_pa\":"<<c.reference_pressure_absolute_pa<<",\"temperature_k\":"<<c.temperature_k<<",\"rho_kg_m3\":"<<d.rho_kg_m3<<",\"mu_pa_s\":"<<ViscosityPaS<<",\"nu_m2_s\":"<<d.nu_m2_s<<",\"sound_speed_m_s\":"<<d.sound_speed_m_s<<",\n";
    o<<"\"hydraulic_diameter_m\":"<<d.hydraulic_diameter_m<<",\"estimated_pressure_drop_pa\":"<<d.estimated_pressure_drop_pa<<",\"estimated_inlet_absolute_pa\":"<<d.estimated_inlet_absolute_pa<<",\"analytic_mean_m_s\":"<<d.analytic_mean_m_s<<",\"analytic_peak_m_s\":"<<d.analytic_peak_m_s<<",\"analytic_mass_flow_kg_s\":"<<d.analytic_mass_flow_kg_s<<",\n";
    o<<"\"velocity_component_bound_m_s\":"<<c.velocity_component_bound_m_s<<",\"reynolds_bound\":"<<d.reynolds_bound<<",\"mach_bound\":"<<d.mach_bound<<",\"knudsen_estimate\":"<<d.knudsen_estimate<<",\"relative_pressure_span\":"<<d.relative_pressure_span<<",\n";
    o<<"\"dt_s\":"<<c.dt_s<<",\"duration_s\":"<<c.duration_s<<",\"steps\":"<<d.steps<<",\"advection_budget\":"<<d.advective_number<<",\"diffusion_budget\":"<<d.diffusive_number<<",\"combined_budget\":"<<d.combined_number<<",\"max_dt_s\":"<<d.max_dt_s<<",\n";
    o<<"\"projection\":{\"max_iterations\":"<<c.projection.max_iterations<<",\"absolute_residual_pa_m2\":"<<c.projection.residual_absolute_pa_m2<<",\"relative_residual\":"<<c.projection.residual_relative<<",\"divergence_linf_s_inv\":"<<c.projection.divergence_linf_s_inv<<",\"mass_balance_relative\":"<<c.projection.mass_balance_relative<<"}\n}\n";return o.str();}
double analytic_velocity(const Contract& c,double y){const auto d=validate(c);range(y,0,c.grid.height_m,"reference y outside channel");return 6*d.analytic_mean_m_s*(y/c.grid.height_m)*(1-y/c.grid.height_m);}
double inlet_face_velocity(const Contract& c,int j){const auto d=validate(c);require(j>=0&&j<c.grid.ny,"inlet face index outside channel");const double a=static_cast<double>(j)/c.grid.ny,b=static_cast<double>(j+1)/c.grid.ny;return 6*d.analytic_mean_m_s*((a+b)/2-(a*a+a*b+b*b)/3);}
double analytic_pressure_gauge(const Contract& c,double x){const auto d=validate(c);range(x,0,c.grid.length_m,"reference x outside channel");return (c.boundaries[1].value-c.reference_pressure_absolute_pa)+d.estimated_pressure_drop_pa*(1-x/c.grid.length_m);}
Layout::Layout(const Contract& c):nx_(c.grid.nx),ny_(c.grid.ny){const auto d=validate(c);dx_=d.dx_m;dy_=d.dy_m;}
std::size_t Layout::pressure_count()const{return static_cast<std::size_t>(nx_)*ny_;}
std::size_t Layout::u_count()const{return static_cast<std::size_t>(nx_+1)*ny_;}
std::size_t Layout::v_count()const{return static_cast<std::size_t>(nx_)*(ny_+1);}
std::size_t Layout::p_index(int i,int j)const{return index(i,j,nx_,ny_);}
std::size_t Layout::u_index(int i,int j)const{return index(i,j,nx_+1,ny_);}
std::size_t Layout::v_index(int i,int j)const{return index(i,j,nx_,ny_+1);}
std::array<double,2> Layout::p_position(int i,int j)const{p_index(i,j);return {{(i+.5)*dx_,(j+.5)*dy_}};}
std::array<double,2> Layout::u_position(int i,int j)const{u_index(i,j);return {{i*dx_,(j+.5)*dy_}};}
std::array<double,2> Layout::v_position(int i,int j)const{v_index(i,j);return {{(i+.5)*dx_,j*dy_}};}
Fields make_initial_fields(const Contract& c){Layout l(c);Fields f;f.pressure_gauge_pa.resize(l.pressure_count(),c.boundaries[1].value-c.reference_pressure_absolute_pa);f.u_m_s.resize(l.u_count(),0);f.v_m_s.resize(l.v_count(),0);
    if(c.initial==InitialCondition::SteadyAnalyticReference){for(int j=0;j<c.grid.ny;++j){for(int i=0;i<c.grid.nx;++i)f.pressure_gauge_pa[l.p_index(i,j)]=analytic_pressure_gauge(c,l.p_position(i,j)[0]);for(int i=0;i<=c.grid.nx;++i)f.u_m_s[l.u_index(i,j)]=inlet_face_velocity(c,j);}}
    else if(c.boundaries[0].kind==BoundaryKind::ParabolicVelocity){for(int j=0;j<c.grid.ny;++j)f.u_m_s[l.u_index(0,j)]=inlet_face_velocity(c,j);} // Startup guess is not divergence-free.
    validate_fields(c,f);return f;
}
void validate_fields(const Contract& c,const Fields& f){const auto d=validate(c);Layout l(c);require(f.pressure_gauge_pa.size()==l.pressure_count()&&f.u_m_s.size()==l.u_count()&&f.v_m_s.size()==l.v_count(),"field dimensions do not match MAC grid");require(f.accepted_steps<=d.steps&&std::isfinite(f.physical_time_s)&&f.physical_time_s>=0&&std::abs(f.physical_time_s-static_cast<double>(f.accepted_steps)*c.dt_s)<=1e-10,"field clock inconsistent with accepted fixed steps");for(double p:f.pressure_gauge_pa){finite(p,"nonfinite pressure field");range(c.reference_pressure_absolute_pa+p,95000,105000,"field absolute pressure outside envelope");require(std::abs(p)/c.reference_pressure_absolute_pa<=.01,"field pressure departs from density reference by more than 1 percent");}double minimum=c.boundaries[1].value,maximum=minimum;if(c.boundaries[0].kind==BoundaryKind::PressureAbsolute){minimum=std::min(minimum,c.boundaries[0].value);maximum=std::max(maximum,c.boundaries[0].value);}for(double p:f.pressure_gauge_pa){minimum=std::min(minimum,c.reference_pressure_absolute_pa+p);maximum=std::max(maximum,c.reference_pressure_absolute_pa+p);}require((maximum-minimum)/minimum<=.01,"field pressure span exceeds constant-density envelope");for(int j=0;j<c.grid.ny;++j)require(f.u_m_s[l.u_index(0,j)]>=0&&f.u_m_s[l.u_index(c.grid.nx,j)]>=0,"open-boundary backflow is unsupported");for(const auto* a:{&f.u_m_s,&f.v_m_s})for(double v:*a)require(std::isfinite(v)&&std::abs(v)<=c.velocity_component_bound_m_s,"field velocity exceeds declared envelope; step must be rejected, not clipped");for(int i=0;i<c.grid.nx;++i)require(f.v_m_s[l.v_index(i,0)]==0&&f.v_m_s[l.v_index(i,c.grid.ny)]==0,"impermeable wall has nonzero normal face velocity");if(c.boundaries[0].kind==BoundaryKind::ParabolicVelocity)for(int j=0;j<c.grid.ny;++j)require(std::abs(f.u_m_s[l.u_index(0,j)]-inlet_face_velocity(c,j))<=1e-12,"prescribed inlet face flux changed");}
}
