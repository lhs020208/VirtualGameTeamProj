#include "powder/plant.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace {
double distance(powder::Grain a,powder::Grain b){return std::hypot(std::hypot(a.x-b.x,a.y-b.y),a.z-b.z);}
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
}
int main(int argc,char** argv){try{
 check(argc==4,"usage: revised-data raw-reference output.json");
 const std::string dir=argv[1];auto data=powder::read_plant_data(dir+"/runtime.plant");
 auto routes=powder::read_plant_routes(dir+"/runtime.routes");
 check(std::abs(data[1].z-.9f)<1e-6,"source rotor pivot");
 std::ifstream charge(dir+"/test_charge.csv");std::string line;std::getline(charge,line);
 std::vector<powder::Grain> seed;unsigned counts[2]{};
 while(std::getline(charge,line)){
  std::replace(line.begin(),line.end(),',',' ');std::istringstream in(line);
  unsigned id,s;std::string supplier;float x,y,z,r,rho;
  check(bool(in>>id>>s>>supplier>>x>>y>>z>>r>>rho)&&id==seed.size()&&s<2&&std::abs(r-.002f)<1e-8,"preserved charge");
  ++counts[s];seed.push_back({x,y,z,float(s)});
 }
 check(counts[0]==10000&&counts[1]==10000,"A/B each 10000");
 powder::plant_set_manual_valves(data,routes,1000,0,0,false,false);powder::plant_set_receiver_valve(data,false);
 powder::GranularConfig c;c.plant=true;c.plant_threaded=true;c.kernel_optimize=true;c.plant_wall_mode=5;
 c.plant_data=data;c.radius=.002f;c.dt=.00001f;c.iterations=8;c.count=unsigned(seed.size());
 double initialProjection=0;
 {powder::GranularGPU gpu(c,seed,std::vector<powder::Grain>(seed.size()));auto b=gpu.inspect_boundaries(seed,0);
  for(size_t i=0;i<b.size();++i)initialProjection=std::max(initialProjection,distance(b[i].position,seed[i]));
  check(initialProjection<1e-6,"initial particles intersect walls");gpu.advance(128,0,false);
  auto p=gpu.snapshot();check(p.size()==20000,"particle count changed");for(auto q:p)check(std::isfinite(q.x+q.y+q.z),"nonfinite initial state");
 }
 powder::plant_set_receiver_valve(data,true);c.plant_data=data;
 std::vector<powder::Grain> p;
 for(int i=0;i<12;++i)p.push_back({3.f+(i%3-1)*.006f,(i/3-1.5f)*.006f,1.25f,float(i%2)});
 for(int i=0;i<24;++i)p.push_back({2.32f+.125f*(i%12),i<12?-.2f:.2f,1.15f,float(i%2)});
 c.count=unsigned(p.size());powder::GranularGPU gpu(c,p,std::vector<powder::Grain>(p.size()));
 // Compare the full replaced surfaces against the independent raw triangle BVH.
 auto raw=powder::read_plant_data(std::string(argv[2])+"/runtime.plant");powder::plant_set_receiver_valve(raw,true);
 auto referenceConfig=c;referenceConfig.plant_data=raw;referenceConfig.plant_wall_mode=0;
 powder::GranularGPU reference(referenceConfig,p,std::vector<powder::Grain>(p.size()));
 std::ifstream file(dir+"/mixer_surface_probes.bin",std::ios::binary);std::vector<powder::Grain> samples;powder::Grain q;
 while(file.read(reinterpret_cast<char*>(&q),sizeof(q)))samples.push_back(q);check(samples.size()>10000,"mixer probe fixture missing");
 double surfaceDelta=0,pathDelta=0;unsigned queries=0;
 for(float angle:{0.f,.35f,1.1f}){
  std::vector<powder::Grain> points=samples;
  for(auto& q:points){if(q.w==1){const float y=q.y-data[1].y,z=q.z-data[1].z;q.y=data[1].y+std::cos(angle)*y-std::sin(angle)*z;q.z=data[1].z+std::sin(angle)*y+std::cos(angle)*z;}q.w=0;}
  auto a=reference.inspect_boundaries(points,angle),b=gpu.inspect_boundaries(points,angle);
  for(size_t i=0;i<a.size();++i)surfaceDelta=std::max(surfaceDelta,distance(a[i].position,b[i].position));
  queries+=unsigned(points.size());
 }
 check(surfaceDelta<2e-5,"hybrid differs from source mixer surface");
 std::vector<powder::Grain> path;
 for(int j=0;j<int(data[0].w);++j){int at=int(data[0].z)+3*j;auto a=data[at],b=data[at+1];path.push_back({(a.x+b.x)/2,(a.y+b.y)/2,(a.z+b.z)/2,0});}
 auto a=reference.inspect_boundaries(path,0),b=gpu.inspect_boundaries(path,0);
 for(size_t i=0;i<a.size();++i)pathDelta=std::max(pathDelta,distance(a[i].position,b[i].position));
 check(pathDelta<2e-5,"upstream path changed");
 std::cout<<"surface queries="<<queries<<" delta="<<surfaceDelta<<"; starting local rotor fixture"<<std::endl;
 for(int i=0;i<15000;i+=16)gpu.advance(16,15,false);
 auto final=gpu.snapshot();unsigned entered=0,inside=0;double moved=0;
 for(size_t i=0;i<final.size();++i){auto q=final[i];check(std::isfinite(q.x+q.y+q.z),"nonfinite mixer state");
  if(i<12&&q.z<1.225f)++entered;
  const float y=q.y,z=q.z-.9f;
  if(q.x>2.25f&&q.x<3.75f&&q.z<1.225f&&std::abs(y)<.323f&&(z>=0||y*y+z*z<.323f*.323f))++inside;
  moved+=distance(q,p[i]);
 }
 check(entered==12&&inside==p.size(),"revised inlet/confinement");
 const float runningAngle=gpu.angle();check(runningAngle>.23f,"rotor did not rotate");
 gpu.advance(128,0,false);check(std::abs(gpu.angle()-runningAngle)<1e-7,"stop did not freeze rotor");
 std::ofstream out(argv[3]);out<<std::setprecision(12)<<"{\"backend\":\"OpenCL\",\"particles\":20000,\"initial_projection_m\":"<<initialProjection
  <<",\"mixer_surface_queries\":"<<queries<<",\"mixer_surface_max_delta_m\":"<<surfaceDelta<<",\"pipe_path_queries\":"<<path.size()
  <<",\"pipe_path_max_delta_m\":"<<pathDelta<<",\"local_fixture_particles\":"<<p.size()<<",\"inlet_passed\":"<<entered<<",\"inside_mixer\":"<<inside
  <<",\"simulated_seconds\":"<<gpu.time()<<",\"rotor_angle_radians\":"<<runningAngle<<",\"rotor_stop_passed\":true,\"displacement_m\":"<<moved<<"}";
 std::cout<<"PASS revised mixer OpenCL boundary, inlet, confinement and start/stop"<<std::endl;
 }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
