#include "powder/clump.hpp"
#include <iostream>
#include <iomanip>
using namespace powder;
int main(int argc,char** argv){try{
 const std::size_t n=argc==2?std::stoull(argv[1]):1000;if(n<2||n>100000)throw std::invalid_argument("convergence count 2..100000");
 ClumpWorld a;a.maximum_bodies=a.maximum_pebbles=n;a.maximum_candidates=4000000;a.cpu_threads=8;
 MaterialContact m;m.law=ContactLaw::Linear;m.linear={10000,.01,2500,.0025,.4};a.pair_models={{{0,0},m},{{0,1},m},{{1,1},m}};a.wall_models={{0,m},{1,m}};
 auto side=static_cast<std::size_t>(std::ceil(std::cbrt(double(n))));
 for(std::size_t i=0;i<n;++i){auto body=make_clump(i+1,static_cast<unsigned>(i%2),{{{},.002}},1500);body.position_m={double(i%side)*.00199999,double((i/side)%side)*.00199999,.01+double(i/(side*side))*.00199999};a.bodies.push_back(body);}
 auto b=a;ClumpReport ar,br;for(int i=0;i<10;++i){ar=a.step(2e-8);br=b.step(1e-8);br=b.step(1e-8);}
 double position=0,velocity=0,max_velocity=0,ea=0,eb=0;
 for(std::size_t i=0;i<n;++i){const auto& x=a.bodies[i];const auto& y=b.bodies[i];position=std::max(position,std::hypot(x.position_m.x-y.position_m.x,x.position_m.y-y.position_m.y,x.position_m.z-y.position_m.z));velocity=std::max(velocity,std::hypot(x.velocity_m_s.x-y.velocity_m_s.x,x.velocity_m_s.y-y.velocity_m_s.y,x.velocity_m_s.z-y.velocity_m_s.z));max_velocity=std::max(max_velocity,std::hypot(y.velocity_m_s.x,y.velocity_m_s.y,y.velocity_m_s.z));ea+=clump_kinetic_energy(x);eb+=clump_kinetic_energy(y);}
 const double energy_error=std::abs(ea-eb)/std::max(eb,1e-30);
 if(position>1e-12||velocity>1e-9+.001*max_velocity||energy_error>.005||ar.pair_contacts!=br.pair_contacts||a.histories()!=b.histories()||ar.mass_kg!=br.mass_kg)throw std::runtime_error("common-time half-dt convergence gate");
 std::cout<<std::setprecision(17)<<"{\"passed\":true,\"count\":"<<n<<",\"common_time_s\":2e-7,\"coarse_dt_s\":2e-8,\"fine_dt_s\":1e-8,\"max_position_difference_m\":"<<position<<",\"max_velocity_difference_m_s\":"<<velocity<<",\"reference_max_velocity\":"<<max_velocity<<",\"kinetic_relative_difference\":"<<energy_error<<",\"contacts\":"<<ar.pair_contacts<<",\"histories\":"<<a.histories()<<"}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
