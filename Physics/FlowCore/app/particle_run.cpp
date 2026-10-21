#include "flow/one_way.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace {
double number(const char* text){std::size_t end=0;std::string s=text;double value=std::stod(s,&end);if(end!=s.size())throw std::invalid_argument("invalid numeric option");return value;}
std::string quote(const std::string& text){std::string result="\"";for(char ch:text){if(ch=='"'||ch=='\\')result+='\\';result+=ch<' '?' ':ch;}return result+'"';}
void write(const std::filesystem::path& p,const std::string& text){std::ofstream f(p);f<<text;f.close();if(!f)throw std::runtime_error("output write failed");}
}
int main(int argc,char** argv){try{
    if(argc!=3&&argc!=6)throw std::invalid_argument("usage: flow_particle_run INPUT.flow NEW_DIRECTORY [diameter_m density_kg_m3 integer_substeps]");
    std::filesystem::path source=argv[1],output=argv[2];if(std::filesystem::file_size(source)>65536)throw std::invalid_argument("input exceeds 64 KiB");
    std::ifstream input(source);if(!input)throw std::runtime_error("cannot read input");const std::string text((std::istreambuf_iterator<char>(input)),{});if(input.bad())throw std::runtime_error("input read failed");
    const auto c=flow::parse_contract(text);flow::OneWaySettings settings;powder::Particle p;p.id=1;p.diameter_m=30e-6;p.material.density_kg_m3=1000;p.position_m={c.grid.length_m/2,c.grid.height_m/2,0};
    if(argc==6){p.diameter_m=number(argv[3]);p.material.density_kg_m3=number(argv[4]);const double sub=number(argv[5]);if(!(sub>=1&&sub<=100000)||sub!=static_cast<int>(sub))throw std::invalid_argument("invalid integer substeps");settings.particle_substeps=static_cast<int>(sub);}
    flow::OneWayState state{flow::make_initial_fields(c),{p}};flow::validate_one_way(c,settings,state);
    if(!std::filesystem::create_directory(output))throw std::invalid_argument("output directory must be new");
    write(output/"input.flow",flow::serialize_contract(c));
    std::ostringstream config;config.imbue(std::locale::classic());config<<std::setprecision(17)<<"{\"version\":\""<<flow::OneWayVersion<<"\",\"mode\":\"one_way_stokes\",\"diameter_m\":"<<p.diameter_m<<",\"density_kg_m3\":"<<p.material.density_kg_m3<<",\"substeps\":"<<settings.particle_substeps<<",\"gravity_m_s2\":[0,-9.81,0],\"initial_position_m\":["<<p.position_m.x<<','<<p.position_m.y<<",0],\"initial_velocity_m_s\":[0,0,0],\"drag_enabled\":true,\"air_reaction_enabled\":false}\n";
    write(output/"particle.json",config.str());
    std::ofstream history(output/"particles.csv");history.imbue(std::locale::classic());history<<std::setprecision(17)<<"air_step,time_s,id,x_m,y_m,vx_m_s,vy_m_s,max_particle_Re,drag_impulse_x_Ns,drag_impulse_y_Ns\n";
    auto record=[&](const flow::OneWayReport& r){const auto& q=state.particles[0];history<<state.air.accepted_steps<<','<<state.air.physical_time_s<<','<<q.id<<','<<q.position_m.x<<','<<q.position_m.y<<','<<q.velocity_m_s.x<<','<<q.velocity_m_s.y<<','<<r.max_particle_reynolds<<','<<r.drag_impulse_n_s.x<<','<<r.drag_impulse_n_s.y<<'\n';if(!history)throw std::runtime_error("history write failure");};
    record({});std::string error;double max_re=0,max_ratio=0;const auto d=flow::validate(c);
    try{while(state.air.accepted_steps<d.steps){auto r=flow::advance_one_way(c,settings,state);max_re=std::max(max_re,r.max_particle_reynolds);max_ratio=std::max(max_ratio,r.max_dt_over_tau);record(r);}}catch(const std::exception& e){error=e.what();}
    history.close();if(!history&&error.empty())error="history close failure";
    std::ostringstream report;report.imbue(std::locale::classic());report<<std::setprecision(17)<<"{\"version\":\""<<flow::OneWayVersion<<"\",\"passed\":"<<(error.empty()?"true":"false")<<",\"coupling\":\"air_to_particle_only\",\"air_reaction_enabled\":false,\"accepted_air_steps\":"<<state.air.accepted_steps<<",\"physical_time_s\":"<<state.air.physical_time_s<<",\"particle_substeps_per_air_step\":"<<settings.particle_substeps<<",\"max_particle_Re\":"<<max_re<<",\"max_dt_over_tau\":"<<max_ratio<<",\"error\":"<<quote(error)<<"}\n";
    write(output/"result.json.tmp",report.str());std::filesystem::rename(output/"result.json.tmp",output/"result.json");
    std::cout<<(error.empty()?"PASS ":"FAILED ")<<output.string()<<'\n';if(!error.empty())std::cerr<<error<<'\n';return error.empty()?0:3;
}catch(const std::exception& e){std::cerr<<"ONE_WAY_REJECTED: "<<e.what()<<'\n';return 2;}}
