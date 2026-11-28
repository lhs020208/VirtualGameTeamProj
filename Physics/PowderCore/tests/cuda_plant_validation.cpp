#include "powder/plant.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
namespace {
double distance(powder::Grain a,powder::Grain b){return std::hypot(double(a.x)-b.x,double(a.y)-b.y,double(a.z)-b.z);}
void require(bool b,const char*s){if(!b)throw std::runtime_error(s);}
}
int main(int argc,char**argv){try{
 require(argc==3||argc==4,"data report.json [CUDA backend 1 or 3]");int backend=argc==4?std::stoi(argv[3]):3;require(backend==1||backend==3,"CUDA backend");std::string dir=argv[1];auto d=powder::read_plant_data(dir+"/runtime.plant");auto routes=powder::read_plant_routes(dir+"/runtime.routes");powder::plant_set_manual_valves(d,routes,1000,0,0,false,false);powder::plant_set_receiver_valve(d,false);
 std::ifstream charge(dir+"/test_charge.csv");std::string line;std::getline(charge,line);std::vector<powder::Grain> seed;while(std::getline(charge,line)){std::replace(line.begin(),line.end(),',',' ');std::istringstream in(line);int id,s;std::string group;float x,y,z,r,rho;in>>id>>s>>group>>x>>y>>z>>r>>rho;seed.push_back({x,y,z,float(s)});}
 powder::GranularConfig c;c.plant=true;c.plant_threaded=true;c.kernel_optimize=true;c.plant_wall_mode=5;c.plant_data=d;c.radius=.002f;c.dt=.00001f;c.iterations=8;c.count=unsigned(seed.size());std::vector<powder::Grain> zero(seed.size());
 powder::GranularGPU cl(c,seed,zero);c.backend=static_cast<powder::GranularBackend>(backend);powder::GranularGPU cu(c,seed,zero);c.backend=static_cast<powder::GranularBackend>(backend+1);powder::GranularGPU graph(c,seed,zero);
 double positionMax=0,normalMax=0,wallVelocityMax=0;size_t queries=0;std::vector<powder::Grain> samples;std::ifstream file(dir+"/surface_probes.bin",std::ios::binary);powder::Grain g;while(file.read(reinterpret_cast<char*>(&g),sizeof(g)))samples.push_back(g);require(!samples.empty(),"missing probes");
 std::ofstream out(argv[2]);out<<std::setprecision(12)<<"{\"boundary\":[";bool first=true;
 for(float angle:{0.f,.35f,1.1f})for(bool shell:{false,true}){
  double pos=0,norm=0,vel=0;size_t count=0;
  for(size_t start=0;start<samples.size();start+=20000){std::vector<powder::Grain> points;for(size_t i=start;i<std::min(start+20000,samples.size());++i){auto a=samples[i];if(a.w==1){float y=a.y-d[1].y,z=a.z-d[1].z;a.y=d[1].y+std::cos(angle)*y-std::sin(angle)*z;a.z=d[1].z+std::sin(angle)*y+std::cos(angle)*z;}a.w=0;points.push_back(a);}auto a=cl.inspect_boundaries(points,angle,false,shell),b=cu.inspect_boundaries(points,angle,false,shell);for(size_t i=0;i<a.size();++i){require(std::isfinite(b[i].position.x+b[i].position.y+b[i].position.z+b[i].normal.x+b[i].normal.y+b[i].normal.z),"nonfinite boundary");pos=std::max(pos,distance(a[i].position,b[i].position));norm=std::max(norm,distance(a[i].normal,b[i].normal));vel=std::max(vel,distance(a[i].velocity,b[i].velocity));}count+=points.size();}
  if(!first)out<<',';first=false;out<<"{\"angle\":"<<angle<<",\"shell\":"<<shell<<",\"queries\":"<<count<<",\"position_max_m\":"<<pos<<",\"normal_max\":"<<norm<<",\"wall_velocity_max\":"<<vel<<"}";out.flush();positionMax=std::max(positionMax,pos);normalMax=std::max(normalMax,norm);wallVelocityMax=std::max(wallVelocityMax,vel);queries+=count;
 }
 out<<"],\"steps\":[";first=true;double stepPos=0,stepVel=0,graphPos=0,graphVel=0;
 for(int gatecase=0;gatecase<3;++gatecase){powder::plant_set_manual_valves(d,routes,1000,3,3,gatecase>=1,gatecase>=1);powder::plant_set_receiver_valve(d,gatecase>=2);cl.update_plant_field(d);cu.update_plant_field(d);graph.update_plant_field(d);cl.set_state(seed,zero);cu.set_state(seed,zero);graph.set_state(seed,zero);
  for(int batch=0;batch<3;++batch){float rpm=batch==1?15.f:0.f;cl.advance(16,rpm,false);cu.advance(16,rpm,false);graph.advance(16,rpm,false);auto a=cl.snapshot(),b=cu.snapshot(),e=graph.snapshot(),av=cl.velocity_snapshot(),bv=cu.velocity_snapshot(),ev=graph.velocity_snapshot();double pm=0,vm=0,pg=0,vg=0;for(size_t i=0;i<a.size();++i){require(a[i].w==b[i].w&&b[i].w==e[i].w&&e[i].w==seed[i].w,"ID/species changed");pm=std::max(pm,distance(a[i],b[i]));vm=std::max(vm,distance(av[i],bv[i]));pg=std::max(pg,distance(b[i],e[i]));vg=std::max(vg,distance(bv[i],ev[i]));}stepPos=std::max(stepPos,pm);stepVel=std::max(stepVel,vm);graphPos=std::max(graphPos,pg);graphVel=std::max(graphVel,vg);if(!first)out<<',';first=false;out<<"{\"gate_case\":"<<gatecase<<",\"batch\":"<<batch<<",\"position_max_m\":"<<pm<<",\"velocity_max_m_s\":"<<vm<<",\"graph_position_max_m\":"<<pg<<",\"graph_velocity_max_m_s\":"<<vg<<"}";}
 }
 bool pass=positionMax<=2e-6&&normalMax<=.002&&wallVelocityMax<=2e-6&&stepPos<=2e-6&&stepVel<=.002&&graphPos<=2e-6&&graphVel<=.002;
 out<<"],\"queries\":"<<queries<<",\"pass\":"<<(pass?"true":"false")<<",\"experimental_validation\":false}";out.close();std::cout<<"boundary pos="<<positionMax<<" normal="<<normalMax<<" wallVelocity="<<wallVelocityMax<<" step pos="<<stepPos<<" vel="<<stepVel<<" graph pos="<<graphPos<<" vel="<<graphVel<<" pass="<<pass<<'\n';return pass?0:2;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
