#include "powder/plant.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
// Isolated particles at the observed stall locations. No production data is written.
int main(int argc,char** argv){try{
 if(argc<6)throw std::runtime_error("data output.csv friction flow_scale steps [incoming_vx] [fixture_index]");
 auto d=powder::read_plant_data(std::string(argv[1])+"/runtime.plant");
 auto routes=powder::read_plant_routes(std::string(argv[1])+"/runtime.routes");
 powder::plant_set_manual_valves(d,routes,1000,3,3,true,true);
 powder::plant_set_receiver_valve(d,false);
 float scale=std::stof(argv[4]);
 for(unsigned j=0;j<unsigned(d[0].w);++j)d[unsigned(d[0].z)+3*j+2].x*=scale;
 std::cout<<std::setprecision(10)<<"weighing_gauge_Pa="<<d[4].z<<" receiver_gauge_Pa="<<d[4].w<<" flow_scale="<<scale<<" (scaled flow is a diagnostic override, not a pressure solution)\n";
 for(const auto& route:routes)if(route.phase==1){for(int at:route.cells){auto a=d[at],b=d[at+1],q=d[at+2];if(a.x>=1.9f)std::cout<<"air_cell "<<a.x<<","<<a.y<<","<<a.z<<" -> "<<b.x<<","<<b.y<<","<<b.z<<" radius="<<a.w<<","<<b.w<<" q="<<q.x<<" inlet_speed="<<q.x/(3.14159265359f*a.w*a.w)<<'\n';}}
 std::vector<powder::Grain> p={{1.95f,0,2.6231f,0},{2.17f,0,2.6201f,0},{2.3f,0,2.6231f,0},{2.69f,0,2.6231f,0},{2.79f,0,2.6231f,0},{2.3f,0,2.682f,0}};
 if(argc>7)p={p.at(std::stoul(argv[7]))}; // Definitive runs isolate one particle from every other fixture.
 std::vector<powder::Grain> v(p.size());for(auto& a:v)a.x=argc>6?std::stof(argv[6]):0;
 powder::GranularConfig c;c.plant=true;c.plant_threaded=true;c.plant_triangle_fast=false;c.plant_wall_mode=3;c.kernel_optimize=true;c.plant_data=d;c.count=unsigned(p.size());c.radius=.002f;c.dt=.00001f;c.iterations=8;c.friction=std::stof(argv[3]);
 powder::GranularGPU gpu(c,p,v);
 std::ofstream out(argv[2]);if(!out)throw std::runtime_error("output");
 out<<std::setprecision(10)<<"step,id,x,y,z,vx,vy,vz,projection_m,nx,ny,nz\n";
 auto save=[&](unsigned step){auto a=gpu.snapshot(),b=gpu.velocity_snapshot();auto hits=gpu.inspect_boundaries(a,0,false,true);for(size_t i=0;i<a.size();++i){auto h=hits[i];double delta=std::hypot(std::hypot(h.position.x-a[i].x,h.position.y-a[i].y),h.position.z-a[i].z);out<<step<<','<<i<<','<<a[i].x<<','<<a[i].y<<','<<a[i].z<<','<<b[i].x<<','<<b[i].y<<','<<b[i].z<<','<<delta<<','<<h.normal.x<<','<<h.normal.y<<','<<h.normal.z<<'\n';}out.flush();};
 save(0);auto start=std::chrono::steady_clock::now();unsigned steps=std::stoul(argv[5]);
 for(unsigned i=0;i<steps;){unsigned n=std::min(1000u,steps-i);for(unsigned k=0;k<n;k+=16)gpu.advance(std::min(16u,n-k),0,false);i+=n;save(i);}
 std::cout<<"PASS "<<argv[2]<<" wall_s="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<std::endl;
 }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
