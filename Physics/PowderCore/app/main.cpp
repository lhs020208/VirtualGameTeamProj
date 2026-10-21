#include "powder/core.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace {
double number(const std::string& s) {
    std::size_t n=0; double v=std::stod(s,&n);
    if(n!=s.size()) throw std::invalid_argument("invalid numeric value: "+s);
    return v;
}
std::uint64_t integer(const std::string& s) {
    if(s.empty()||s.find_first_not_of("0123456789")!=std::string::npos)
        throw std::invalid_argument("expected unsigned integer: "+s);
    return std::stoull(s);
}
}
int main(int argc,char** argv) {
    try {
        powder::RunConfig c; powder::Particle p; std::string output;
        powder::Vec3 gravity{0,0,-9.81}, force;
        powder::PlaneContact plane; bool use_plane=false;
        std::unordered_set<std::string> seen;
        for(int i=1;i<argc;++i) {
            const std::string key=argv[i];
            if(key=="--help") {
                std::cout<<"powder_run [--dt seconds] [--steps count] [--seed integer] [--diameter metres] [--density kg/m3] [--output file.json]\nTranslation; optional --plane-z (m), --stiffness (N/m), --damping (Ns/m). Options: --px/py/pz (m), --vx/vy/vz (m/s), --gx/gy/gz (m/s2), --fx/fy/fz (N). Default gravity Z=-9.81.\n"; return 0;
            }
            if(!seen.insert(key).second) throw std::invalid_argument("duplicate option: "+key);
            if(i+1==argc) throw std::invalid_argument("missing value for "+key);
            const std::string value=argv[++i];
            if(key=="--dt") c.dt_s=number(value);
            else if(key=="--steps") c.steps=integer(value);
            else if(key=="--seed") c.seed=integer(value);
            else if(key=="--diameter") p.diameter_m=number(value);
            else if(key=="--density") p.material.density_kg_m3=number(value);
            else if(key=="--px") p.position_m.x=number(value);
            else if(key=="--py") p.position_m.y=number(value);
            else if(key=="--pz") p.position_m.z=number(value);
            else if(key=="--vx") p.velocity_m_s.x=number(value);
            else if(key=="--vy") p.velocity_m_s.y=number(value);
            else if(key=="--vz") p.velocity_m_s.z=number(value);
            else if(key=="--gx") gravity.x=number(value);
            else if(key=="--gy") gravity.y=number(value);
            else if(key=="--gz") gravity.z=number(value);
            else if(key=="--fx") force.x=number(value);
            else if(key=="--fy") force.y=number(value);
            else if(key=="--fz") force.z=number(value);
            else if(key=="--plane-z") { plane.height_m=number(value); use_plane=true; }
            else if(key=="--stiffness") plane.stiffness_n_m=number(value);
            else if(key=="--damping") plane.damping_n_s_m=number(value);
            else if(key=="--output") output=value;
            else throw std::invalid_argument("unknown option: "+key);
        }
        powder::validate(p); powder::validate(c);
        if(c.steps>10000000) throw std::invalid_argument("runner limit is 10000000 steps");
        const auto props=powder::sphere_properties(p.diameter_m,p.material);
        if(!use_plane && (seen.count("--stiffness") || seen.count("--damping")))
            throw std::invalid_argument("contact parameters require --plane-z");
        if(use_plane) {
            powder::validate(plane);
            if(c.dt_s>powder::contact_step_limit_s(p,plane)) throw std::invalid_argument("dt_s exceeds conservative contact step limit");
        }
        const powder::Particle initial=p;
        powder::ForceAccumulator forces;
        // Validate loads even for an empty run.
        forces.add(force); forces.add_gravity(props.mass_kg,gravity); forces.clear();
        powder::FixedClock clock(c);
        while(clock.tick()<c.steps) {
            forces.add(force); forces.add_gravity(props.mass_kg,gravity);
            if(use_plane) powder::step_with_plane(p,forces,plane,c.dt_s);
            else powder::integrate_translation(p,forces,c.dt_s);
            clock.advance();
        }
        std::ostringstream json; json.imbue(std::locale::classic()); json<<std::setprecision(17);
        json<<"{\n  \"schema_version\": 3,\n  \"core_version\": \""<<powder::Version
            <<"\",\n  \"model\": \""<<(use_plane ? "plane_linear_compression_dashpot" : powder::Model)<<"\",\n  \"units\": \"m,kg,s,rad\",\n  \"seed\": "<<c.seed
            <<",\n  \"randomness_used\": false,\n  \"dt_s\": "<<c.dt_s<<",\n  \"requested_steps\": "<<c.steps
            <<",\n  \"completed_steps\": "<<clock.tick()<<",\n  \"simulation_time_s\": "<<clock.time_s()
            <<",\n  \"particle\": {\"id\": "<<p.id<<", \"diameter_m\": "<<p.diameter_m
            <<", \"density_kg_m3\": "<<p.material.density_kg_m3<<", \"mass_kg\": "<<props.mass_kg
            <<", \"inertia_kg_m2\": "<<props.inertia_kg_m2
            <<", \"position_m\": ["<<p.position_m.x<<","<<p.position_m.y<<","<<p.position_m.z
            <<"], \"velocity_m_s\": ["<<p.velocity_m_s.x<<","<<p.velocity_m_s.y<<","<<p.velocity_m_s.z
            <<"], \"angular_velocity_rad_s\": [0,0,0], \"orientation_wxyz\": [1,0,0,0]},\n"
            <<"  \"initial_position_m\": ["<<initial.position_m.x<<","<<initial.position_m.y<<","<<initial.position_m.z<<"],\n"
            <<"  \"initial_velocity_m_s\": ["<<initial.velocity_m_s.x<<","<<initial.velocity_m_s.y<<","<<initial.velocity_m_s.z<<"],\n"
            <<"  \"gravity_m_s2\": ["<<gravity.x<<","<<gravity.y<<","<<gravity.z<<"],\n"
            <<"  \"constant_force_n\": ["<<force.x<<","<<force.y<<","<<force.z<<"],\n  \"plane_enabled\": "<<(use_plane ? "true" : "false")
            <<",\n  \"plane_height_m\": "<<plane.height_m<<",\n  \"stiffness_n_m\": "<<plane.stiffness_n_m
            <<",\n  \"damping_n_s_m\": "<<plane.damping_n_s_m<<"\n}\n";
        if(!output.empty()) {
            std::ofstream f(output,std::ios::binary);
            if(!f) throw std::runtime_error("cannot open output file: "+output);
            f<<json.str(); f.close();
            if(!f) throw std::runtime_error("failed writing output file: "+output);
        }
        std::cout<<json.str(); return 0;
    } catch(const std::exception& e) { std::cerr<<"ERROR: "<<e.what()<<'\n'; return 1; }
}
