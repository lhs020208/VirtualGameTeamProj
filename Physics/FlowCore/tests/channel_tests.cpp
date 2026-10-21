#include "flow/channel.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
void rejects(const std::function<void()>& f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,"invalid input accepted");}
flow::Contract contract(int ny=8){flow::Contract c;c.grid.nx=2*ny;c.grid.ny=ny;c.grid.length_m=.02;c.grid.reporting_depth_m=.001;c.boundaries[0].value=101325.0008688;c.initial=flow::InitialCondition::SteadyAnalyticReference;c.duration_s=.2;return c;}
flow::ChannelSettings settings(){flow::ChannelSettings s;s.inlet_x_m=.009;s.outlet_x_m=.011;s.coupling.coupling_substeps=10;return s;}
struct Outcome{double residence,pressure,mass;};
Outcome run(int ny,int sub,std::ostream& out){auto c=contract(ny);c.grid.reporting_depth_m=1;auto s=settings();s.coupling.coupling_substeps=sub;auto state=flow::make_channel(c,s);flow::ChannelReport r;
while(state.records[0].status!=flow::TransitStatus::Exited)r=flow::advance_channel(c,s,state);
const auto& t=state.records[0];double residence=t.crossing_time_s-t.entry_time_s;out<<"flight_"<<ny<<"_"<<sub<<','<<residence<<','<<r.probe_inlet_gauge_pa-r.probe_outlet_gauge_pa<<'\n';return {residence,r.probe_inlet_gauge_pa-r.probe_outlet_gauge_pa,r.balance.exited_mass_kg};}
}
int main(int argc,char** argv){try{std::ofstream file;std::ostream* stream=&std::cout;if(argc==2){file.open(argv[1]);check(bool(file),"metrics open");stream=&file;}auto& out=*stream;out<<std::setprecision(17)<<"case,value,auxiliary\n";
auto c=contract();auto s=settings();s.releases={{1,0},{2,.01},{3,.02}};auto state=flow::make_channel(c,s),repeat=state;double maxair=0,maxexchange=0;bool queued=false,buffer=false;flow::ChannelReport report;
for(std::uint64_t k=0;k<flow::validate(c).steps;++k){report=flow::advance_channel(c,s,state);flow::advance_channel(c,s,repeat);check(state.coupled.air.u_m_s==repeat.coupled.air.u_m_s&&state.air_in_mass_kg==repeat.air_in_mass_kg,"repeat mismatch");queued=queued||report.balance.due_waiting>0;buffer=buffer||report.balance.buffer_volume_m3>0;maxair=std::max(maxair,report.balance.air_mass_residual_kg);maxexchange=std::max(maxexchange,report.max_exchange_residual_n_s);check(report.balance.particle_mass_residual_kg<1e-22,"particle balance");}
check(queued&&buffer&&report.balance.exited==3&&report.balance.pending==0&&report.balance.active==0,"queue or all arrivals failed");check(state.records[1].entry_time_s>.01,"blocked source not delayed");
for(const auto& t:state.records){check(t.removal_time_s>=t.crossing_time_s&&t.removal_time_s-t.crossing_time_s<=c.dt_s/s.coupling.coupling_substeps+1e-12,"exit resolution");out<<"residence_"<<t.request.id<<','<<t.crossing_time_s-t.entry_time_s<<','<<t.entry_time_s-t.request.requested_time_s<<'\n';}
out<<"air_mass_residual_kg,"<<maxair<<",0\nexchange_residual_Ns,"<<maxexchange<<",0\narrived_mass_kg,"<<report.balance.exited_mass_kg<<",3\n";
Outcome grids[3];int n=0;for(int ny:{8,16,32})grids[n++]=run(ny,10,out);const double analytic=.002/.02875;double e0=std::abs(grids[0].residence-analytic),e1=std::abs(grids[1].residence-analytic),e2=std::abs(grids[2].residence-analytic);check(e1<e0&&e2<e1&&e2/analytic<.002,"Poiseuille flight grid convergence");check(grids[0].mass==grids[1].mass&&grids[1].mass==grids[2].mass,"arrival mass grid sensitivity");
check(std::abs(grids[2].pressure-grids[1].pressure)<std::abs(grids[1].pressure-grids[0].pressure),"pressure grid differences not shrinking");
const double dp=(c.boundaries[0].value-c.boundaries[1].value)*.002/.02;check(std::abs(grids[2].pressure/dp-1)<.002,"pressure reference");out<<"flight_finest_relative_error,"<<e2/analytic<<",0\npressure_finest_relative_error,"<<std::abs(grids[2].pressure/dp-1)<<",0\n";
auto t0=run(8,5,out),t1=grids[0],t2=run(8,20,out);check(std::abs(t2.residence-t1.residence)<std::abs(t1.residence-t0.residence),"residence time refinement");out<<"time_fine_relative_difference,"<<std::abs(t2.residence-t1.residence)/t2.residence<<",0\n";
auto bad=s;bad.releases[1].id=1;rejects([&]{flow::make_channel(c,bad);});bad=s;bad.inlet_x_m=0;rejects([&]{flow::make_channel(c,bad);});bad=s;bad.releases[0].velocity_x_m_s=-.01;rejects([&]{flow::make_channel(c,bad);});
auto initial=flow::make_channel(c,s);auto broken=c;broken.projection.max_iterations=1;rejects([&]{flow::advance_channel(broken,s,initial);});check(initial.coupled.air.accepted_steps==0&&initial.coupled.particles.empty()&&initial.records[0].status==flow::TransitStatus::Pending&&initial.air_in_mass_kg==0,"failed injection was committed");
auto reordered=s;std::reverse(reordered.releases.begin(),reordered.releases.end());auto a=flow::make_channel(c,s),b=flow::make_channel(c,reordered);flow::advance_channel(c,s,a);flow::advance_channel(c,reordered,b);check(a.coupled.air.u_m_s==b.coupled.air.u_m_s,"release order dependence");bad=s;bad.releases[0].diameter_m=31e-6;rejects([&]{flow::advance_channel(c,bad,a);});
std::cout<<"PASS channel\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
