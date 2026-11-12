#include "powder/plant_pressure.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <chrono>
int main(int argc,char**argv){try{
 if(argc!=5 && argc!=6)throw std::runtime_error("data input.csv output.csv mode(B/WH/RH_open/RH_closed) [steps]");
 auto start=std::chrono::steady_clock::now();auto d=powder::read_plant_data(std::string(argv[1])+"/runtime.plant");auto routes=powder::read_plant_routes(std::string(argv[1])+"/runtime.routes");std::string mode=argv[4];
 powder::PlantPressureState ps;
 if(mode=="B")ps=powder::plant_set_regulated_pressures(d,routes,{500,-500,0},2,2,false,false,false);
 else if(mode=="WH")ps=powder::plant_set_regulated_pressures(d,routes,{0,500,-1000},0,0,true,true,false);
 else ps=powder::plant_set_regulated_pressures(d,routes,{0,0,0},0,0,false,false,mode=="RH_open");
 std::ofstream air(std::string(argv[3])+".air.csv");air<<std::setprecision(12)<<"at,ax,ay,az,ra,bx,by,bz,rb,flow,loss,kind,phase\n";for(int j=0;j<int(d[0].w);++j){int at=int(d[0].z)+3*j;auto a=d[at],b=d[at+1],q=d[at+2];air<<at<<','<<a.x<<','<<a.y<<','<<a.z<<','<<a.w<<','<<b.x<<','<<b.y<<','<<b.z<<','<<b.w<<','<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<'\n';}
 std::ifstream in(argv[2]);std::string line;std::getline(in,line);std::vector<unsigned> ids;std::vector<powder::Grain> p;
 while(std::getline(in,line)){std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);unsigned id;powder::Grain q;if(!(row>>id>>q.w>>q.x>>q.y>>q.z))throw std::runtime_error("input row");ids.push_back(id);p.push_back(q);}
 powder::GranularConfig c;c.plant=true;c.plant_threaded=true;c.kernel_optimize=true;c.plant_wall_mode=5;c.plant_data=d;c.radius=.002f;c.dt=.00001f;c.iterations=8;c.count=unsigned(p.size());
 powder::GranularGPU g(c,p,std::vector<powder::Grain>(p.size()));std::ofstream out(argv[3]);out<<std::setprecision(12)<<"sim_s,id,species,x,y,z,vx,vy,vz,nx,ny,nz,projected_x,projected_y,projected_z\n";
 const int steps=argc==6?std::stoi(argv[5]):2000;
 if(steps<=0||steps%100)throw std::runtime_error("steps must be a positive multiple of 100");
 const int stride=steps>=100000?1000:100;
 for(int step=0;step<=steps;step+=std::min(stride,steps-step)){auto q=g.snapshot();auto v=g.velocity_snapshot();auto h=g.inspect_boundaries(q,0,false,true);for(size_t i=0;i<q.size();++i)out<<g.time()<<','<<ids[i]<<','<<q[i].w<<','<<q[i].x<<','<<q[i].y<<','<<q[i].z<<','<<v[i].x<<','<<v[i].y<<','<<v[i].z<<','<<h[i].normal.x<<','<<h[i].normal.y<<','<<h[i].normal.z<<','<<h[i].position.x<<','<<h[i].position.y<<','<<h[i].position.z<<'\n';if(step==steps)break;g.advance(std::min(stride,steps-step),0,false);}
 std::cout<<mode<<" count="<<p.size()<<" transfer_m3s="<<ps.transfer_m3s<<" physical_s="<<g.time()<<" wall_s="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"\nIndependent residual subset, zero initial velocity; NOT a resumed full run.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
