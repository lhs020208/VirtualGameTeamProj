#include "flow/two_way.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace {
void write(const std::filesystem::path& p,const std::string& s){std::ofstream f(p);f<<s;f.close();if(!f)throw std::runtime_error("output write failed");}
std::string quote(const std::string& s){std::string q="\"";for(char ch:s){if(ch=='"'||ch=='\\')q+='\\';q+=ch<' '?' ':ch;}return q+'"';}
int integer(const char* text){std::string s=text;std::size_t end=0;int n=std::stoi(s,&end);if(end!=s.size())throw std::invalid_argument("integer substeps required");return n;}
}
int main(int argc,char** argv){try{
    if(argc<3||argc>5)throw std::invalid_argument("usage: flow_two_way_run INPUT.flow NEW_DIRECTORY [coupling_substeps [reaction_on_0_or_1]]");
    const std::filesystem::path input=argv[1],output=argv[2];if(std::filesystem::file_size(input)>65536)throw std::invalid_argument("input exceeds 64 KiB");
    std::ifstream in(input);if(!in)throw std::runtime_error("cannot read input");const std::string text((std::istreambuf_iterator<char>(in)),{});if(in.bad())throw std::runtime_error("input read failed");
    const auto c=flow::parse_contract(text);flow::TwoWaySettings settings;if(argc>=4)settings.coupling_substeps=integer(argv[3]);if(argc==5){int flag=integer(argv[4]);if(flag!=0&&flag!=1)throw std::invalid_argument("reaction flag must be 0 or 1");settings.reaction_enabled=flag==1;}
    powder::Particle p;p.id=1;p.diameter_m=30e-6;p.material.density_kg_m3=1000;p.position_m={c.grid.length_m/2,c.grid.height_m/2,0};flow::TwoWayState state{flow::make_initial_fields(c),{p}};flow::validate_two_way(c,settings,state);
    if(!std::filesystem::create_directory(output))throw std::invalid_argument("output directory must be new");write(output/"input.flow",flow::serialize_contract(c));
    std::ostringstream config;config.imbue(std::locale::classic());config<<std::setprecision(17)<<"{\"version\":"<<quote(flow::TwoWayVersion)<<",\"coupling_substeps\":"<<settings.coupling_substeps<<",\"kernel_radius_m\":"<<settings.kernel_radius_m<<",\"diameter_m\":"<<p.diameter_m<<",\"density_kg_m3\":1000,\"gravity_m_s2\":[0,0,0],\"initial_position_m\":["<<p.position_m.x<<','<<p.position_m.y<<",0],\"initial_velocity_m_s\":[0,0,0],\"reaction_enabled\":"<<(settings.reaction_enabled?"true":"false")<<",\"finite_porosity_equations\":false,\"max_solid_fraction\":0.001}\n";write(output/"coupling.json",config.str());
    std::ofstream history(output/"exchange.csv");history.imbue(std::locale::classic());history<<std::setprecision(17)<<"step,time_s,x_m,y_m,vx_m_s,vy_m_s,particle_drag_Jx_Ns,particle_drag_Jy_Ns,gas_source_Jx_Ns,gas_source_Jy_Ns,max_exchange_error_Ns,max_solid_fraction,max_particle_Re,max_dt_over_exchange_time,last_substep_divergence_s_inv\n";
    double maxerror=0,maxalpha=0;std::string error;const auto d=flow::validate(c);
    try{while(state.air.accepted_steps<d.steps){auto r=flow::advance_two_way(c,settings,state);maxerror=std::max(maxerror,r.exchange_residual_n_s);maxalpha=std::max(maxalpha,r.volume.max_fraction);const auto& q=state.particles[0];history<<state.air.accepted_steps<<','<<state.air.physical_time_s<<','<<q.position_m.x<<','<<q.position_m.y<<','<<q.velocity_m_s.x<<','<<q.velocity_m_s.y<<','<<r.particle_drag_impulse_n_s.x<<','<<r.particle_drag_impulse_n_s.y<<','<<r.gas_source_impulse_n_s.x<<','<<r.gas_source_impulse_n_s.y<<','<<r.exchange_residual_n_s<<','<<r.volume.max_fraction<<','<<r.max_particle_reynolds<<','<<r.max_dt_over_exchange_time<<','<<r.air.divergence_linf_s_inv<<'\n';if(!history)throw std::runtime_error("history write failed");}}catch(const std::exception& e){error=e.what();}
    history.close();if(!history&&error.empty())error="history close failed";
    const auto volume=flow::map_particle_volume(c,state.particles,settings.kernel_radius_m);std::ostringstream cells;cells.imbue(std::locale::classic());cells<<std::setprecision(17)<<"i,j,x_m,y_m,solid_fraction,void_fraction,pressure_gauge_pa\n";flow::Layout layout(c);
    for(int j=0;j<c.grid.ny;++j)for(int i=0;i<c.grid.nx;++i){auto pos=layout.p_position(i,j);auto k=layout.p_index(i,j);cells<<i<<','<<j<<','<<pos[0]<<','<<pos[1]<<','<<volume.solid_fraction[k]<<','<<1-volume.solid_fraction[k]<<','<<state.air.pressure_gauge_pa[k]<<'\n';}write(output/"volume_pressure.csv",cells.str());
    std::ostringstream faces;faces.imbue(std::locale::classic());faces<<std::setprecision(17)<<"component,i,j,x_m,y_m,velocity_m_s\n";
    for(int j=0;j<c.grid.ny;++j)for(int i=0;i<=c.grid.nx;++i){auto xy=layout.u_position(i,j);faces<<"u,"<<i<<','<<j<<','<<xy[0]<<','<<xy[1]<<','<<state.air.u_m_s[layout.u_index(i,j)]<<'\n';}
    for(int j=0;j<=c.grid.ny;++j)for(int i=0;i<c.grid.nx;++i){auto xy=layout.v_position(i,j);faces<<"v,"<<i<<','<<j<<','<<xy[0]<<','<<xy[1]<<','<<state.air.v_m_s[layout.v_index(i,j)]<<'\n';}write(output/"air_velocity.csv",faces.str());
    std::ostringstream report;report.imbue(std::locale::classic());report<<std::setprecision(17)<<"{\"version\":"<<quote(flow::TwoWayVersion)<<",\"passed\":"<<(error.empty()?"true":"false")<<",\"reaction_enabled\":"<<(settings.reaction_enabled?"true":"false")<<",\"model\":\"dilute_regularized_point_force\",\"finite_porosity_equations\":false,\"accepted_steps\":"<<state.air.accepted_steps<<",\"physical_time_s\":"<<state.air.physical_time_s<<",\"max_exchange_error_Ns\":"<<maxerror<<",\"max_solid_fraction\":"<<maxalpha<<",\"particle_volume_m3\":"<<volume.particle_volume_m3<<",\"mapped_volume_m3\":"<<volume.grid_volume_m3<<",\"final_particle_vx_m_s\":"<<state.particles[0].velocity_m_s.x<<",\"error\":"<<quote(error)<<"}\n";
    write(output/"result.json.tmp",report.str());std::filesystem::rename(output/"result.json.tmp",output/"result.json");std::cout<<(error.empty()?"PASS ":"FAILED ")<<output.string()<<'\n';if(!error.empty())std::cerr<<error<<'\n';return error.empty()?0:3;
}catch(const std::exception& e){std::cerr<<"TWO_WAY_REJECTED: "<<e.what()<<'\n';return 2;}}
