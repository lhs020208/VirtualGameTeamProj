#pragma once
#include "powder/clump.hpp"
#include <array>
#include <set>
namespace powder {
// Small CPU reference, 3D Cartesian isothermal volume-averaged gas / rigid DEM.
struct GasConfig {
 bool slip_walls=false;
 std::array<int,3> cells{16,6,6};double h=.02,temperature_k=293.15,initial_pressure_pa=101325;
 // -/+ faces of X,Y,Z; zero = impermeable no-slip wall, positive = pressure reservoir.
 std::array<double,6> boundary_pa{101325,101300,0,0,0,0};
 void validate()const;double viscosity()const;
};
struct GasCell {double mass=0,epsilon=1;Vec3 momentum;bool solid=false;};
// A physical pressure opening on ONE fluid-cell face, including a face beside
// a solid mask cell. It must not replace a fluid/fluid interior face.
struct GasPressurePort {int cell=0,axis=0,side=0;double absolute_pa=101325;};
struct GasReport {double mass=0,boundary_mass_out=0,min_epsilon=1,max_speed=0,cfl=0;};
struct GasGrid {
 GasConfig config;std::vector<GasCell> cells;double boundary_mass_out=0;
 std::vector<GasPressurePort> pressure_ports;
 explicit GasGrid(GasConfig c={});
 int index(int x,int y,int z)const;Vec3 centre(int i)const;Vec3 extent()const;
 double pressure(int i)const;Vec3 velocity(int i)const;double mass()const;
 void validate_pressure_ports()const;
 double reservoir_pressure(int fluid_cell,int axis,int side)const;
 Vec3 pressure_gradient(int fluid_cell)const;
 void initialize_pressure();
 // Positive normalized CIC, excludes gas-solid cells, conservative volume/force mapping.
 std::vector<std::pair<int,double>> stencil(Vec3 p)const;
 void map_volume(const std::vector<ClumpBody>& bodies);
 double explicit_dt_limit(double cfl=.39)const;
 GasReport step(double dt,Vec3 gravity={});
};
struct CoupledReport {double time=0,gas_mass=0,gas_balance_error=0,particle_mass=0,particle_balance_error=0,exchange_error=0,max_overlap=0,max_speed=0,min_epsilon=1;std::size_t count=0;};
class CoupledWorld {
public:
 struct Timing {double gas_s=0,coupling_s=0,dem_s=0,commit_s=0;};
 Timing timing;
 GasGrid gas;ClumpWorld dem;bool two_way=true;
 double minimum_step_s=0;
 double initial_gas_mass=0,injected_mass=0,exited_mass=0;std::array<double,6> exited_by_face{};
 std::map<std::uint32_t,double> injected_by_type,exited_by_type;
 std::set<ParticleId> used_ids;
 explicit CoupledWorld(GasConfig config={});
 void initialize(); // Call only after setting geometry and initial particles.
 void inject(const std::vector<ClumpBody>& bodies);
 CoupledReport step(double dt); // Transactional gas+DEM commit.
 CoupledReport report()const;
private:double exchange_error_=0,max_overlap_=0;
};
// Same voxel mask and rigid boxes for gas/DEM. Call before initialize.
void add_coupled_obstacle(CoupledWorld&,std::array<int,3> first,std::array<int,3> end);
CoupledWorld coupled_demo(unsigned count=64,double pressure_drop_pa=25,double gamma=0,double temperature_k=293.15,bool obstacle=false,int flow_axis=0);
// Shared user-visible integration fixtures. 0 remains the manual coupled_demo.
// 1=transport, 2=filter retention, 3=blocked transport, 4=no-air transport,
// 5=filter without adhesion; 6=partial obstruction. All are virtual small-scale numerical experiments.
CoupledWorld coupled_experiment(int scenario);
CoupledReport step_coupled_experiment(CoupledWorld&,int scenario,double dt);
const char* coupled_experiment_stage(int scenario,double time);
double coupled_filter_mass(const CoupledWorld&);
double dense_drag_beta(double diameter,double solid_density,double gas_density,double viscosity,double epsilon,double slip);
}
