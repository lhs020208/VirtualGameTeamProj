#include "flow/solver.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace {
void write(const std::filesystem::path& path,const std::string& text){
    std::ofstream out(path,std::ios::binary);out<<text;out.close();if(!out)throw std::runtime_error("failed to write output");
}
void fields(const std::filesystem::path& path,const flow::Contract& c,const flow::Fields& f){
    std::ofstream out(path);out.imbue(std::locale::classic());out<<std::setprecision(17)<<"field,i,j,x_m,y_m,value_SI\n";flow::Layout l(c);
    for(int j=0;j<c.grid.ny;++j)for(int i=0;i<c.grid.nx;++i){auto xy=l.p_position(i,j);out<<"pressure_gauge_pa,"<<i<<','<<j<<','<<xy[0]<<','<<xy[1]<<','<<f.pressure_gauge_pa[l.p_index(i,j)]<<'\n';}
    for(int j=0;j<c.grid.ny;++j)for(int i=0;i<=c.grid.nx;++i){auto xy=l.u_position(i,j);out<<"u_m_s,"<<i<<','<<j<<','<<xy[0]<<','<<xy[1]<<','<<f.u_m_s[l.u_index(i,j)]<<'\n';}
    for(int j=0;j<=c.grid.ny;++j)for(int i=0;i<c.grid.nx;++i){auto xy=l.v_position(i,j);out<<"v_m_s,"<<i<<','<<j<<','<<xy[0]<<','<<xy[1]<<','<<f.v_m_s[l.v_index(i,j)]<<'\n';}
    out.close();if(!out)throw std::runtime_error("failed to write field snapshot");
}
std::string quoted(const std::string& text){std::string out="\"";for(unsigned char ch:text){if(ch=='"'||ch=='\\')out+='\\';if(ch>=32)out+=static_cast<char>(ch);else out+=' ';}return out+'"';}
}
int main(int argc,char** argv){try{
    if(argc!=3)throw std::invalid_argument("usage: flow_run INPUT.flow NEW_OUTPUT_DIRECTORY");
    const std::filesystem::path input=argv[1],output=argv[2];
    if(std::filesystem::file_size(input)>65536)throw std::invalid_argument("input exceeds 64 KiB");
    std::ifstream in(input,std::ios::binary);if(!in)throw std::runtime_error("cannot read input");std::string text((std::istreambuf_iterator<char>(in)),{});
    if(in.bad())throw std::runtime_error("input read error");
    const auto c=flow::parse_contract(text);const auto d=flow::validate(c);auto f=flow::make_initial_fields(c);
    if(!std::filesystem::create_directory(output))throw std::invalid_argument("output directory must be new; refusing overwrite");
    write(output/"input.flow",flow::serialize_contract(c));write(output/"contract.json",flow::contract_report_json(c));
    std::ofstream history(output/"history.csv");history.imbue(std::locale::classic());history<<std::setprecision(17)<<"accepted_step,physical_time_s,pressure_iterations,residual_pa_m2,divergence_linf_s_inv,inlet_kg_s,outlet_kg_s,mass_balance_relative,max_velocity_component_m_s,kinetic_energy_j\n";
    if(!history)throw std::runtime_error("cannot create history");
    const auto started=std::chrono::steady_clock::now();std::string error;flow::StepReport last;double maxdiv=0,maxmass=0,maxresidual=0;std::uint64_t iterations=0;
    try{
        while(f.accepted_steps<d.steps){last=flow::advance(c,f);maxdiv=std::max(maxdiv,last.divergence_linf_s_inv);maxmass=std::max(maxmass,last.mass_balance_relative);maxresidual=std::max(maxresidual,last.pressure_residual_pa_m2);iterations+=last.pressure_iterations;
            history<<f.accepted_steps<<','<<f.physical_time_s<<','<<last.pressure_iterations<<','<<last.pressure_residual_pa_m2<<','<<last.divergence_linf_s_inv<<','<<last.inlet_mass_kg_s<<','<<last.outlet_mass_kg_s<<','<<last.mass_balance_relative<<','<<last.max_speed_m_s<<','<<last.kinetic_energy_j<<'\n';
            if(!history)throw std::runtime_error("history write failed");
        }
    }catch(const std::exception& e){error=e.what();}
    history.close();if(!history&&error.empty())error="history close failed";
    fields(output/"last_accepted_fields.csv",c,f);
    const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
    std::ostringstream report;report.imbue(std::locale::classic());report<<std::setprecision(17)<<"{\n\"schema\":1,\"flow_core_version\":"<<quoted(flow::SolverVersion)<<",\"solver_implemented\":true,\"result_kind\":\"computed_air_only_MAC_fields\",\"passed\":"<<(error.empty()?"true":"false")<<",\n\"accepted_steps\":"<<f.accepted_steps<<",\"requested_steps\":"<<d.steps<<",\"physical_time_s\":"<<f.physical_time_s<<",\"wall_time_s\":"<<elapsed<<",\n\"max_divergence_linf_s_inv\":"<<maxdiv<<",\"max_mass_balance_relative\":"<<maxmass<<",\"max_pressure_residual_pa_m2\":"<<maxresidual<<",\"total_pressure_iterations\":"<<iterations<<",\n\"last_inlet_mass_kg_s\":"<<last.inlet_mass_kg_s<<",\"last_outlet_mass_kg_s\":"<<last.outlet_mass_kg_s<<",\"error\":"<<quoted(error)<<"\n}\n";
    write(output/"result.json.tmp",report.str());std::filesystem::rename(output/"result.json.tmp",output/"result.json");
    std::cout<<(error.empty()?"PASS ":"FAILED ")<<output.string()<<" physical_time_s="<<f.physical_time_s<<'\n';
    if(!error.empty())std::cerr<<error<<'\n';return error.empty()?0:3;
}catch(const std::exception& e){std::cerr<<"FLOW_RUN_REJECTED: "<<e.what()<<'\n';return 2;}}
