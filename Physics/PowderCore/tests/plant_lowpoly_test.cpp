#include "powder/plant.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
int main(int argc,char**argv){try{
 if(argc!=3&&argc!=4)throw std::runtime_error("data output.json [original triangle runtime directory]");
 std::string dir=argv[1];auto d=powder::read_plant_data(dir+"/runtime.plant");
 auto routes=powder::read_plant_routes(dir+"/runtime.routes");
 std::ifstream f(dir+"/test_charge.csv");std::string line;std::getline(f,line);std::vector<powder::Grain> seed;
 unsigned counts[2]{};while(std::getline(f,line)){std::replace(line.begin(),line.end(),',',' ');std::istringstream r(line);unsigned id,s;std::string supplier;float x,y,z,rad,rho;if(!(r>>id>>s>>supplier>>x>>y>>z>>rad>>rho)||id!=seed.size()||s>1||std::abs(rad-.002f)>1e-8)throw std::runtime_error("charge");++counts[s];seed.push_back({x,y,z,float(s)});}
 if(counts[0]!=10000||counts[1]!=10000)throw std::runtime_error("counts");
 powder::plant_set_manual_valves(d,routes,1000,0,0,false,false);powder::plant_set_receiver_valve(d,false);
 powder::GranularConfig c;c.plant=true;c.plant_threaded=true;c.kernel_optimize=true;c.plant_wall_mode=5;c.plant_data=d;c.radius=.002f;c.dt=.00001f;c.iterations=8;c.count=unsigned(seed.size());
 double maxInitial=0;{powder::GranularGPU gpu(c,seed,std::vector<powder::Grain>(seed.size()));auto h=gpu.inspect_boundaries(seed,0);for(size_t i=0;i<h.size();++i)maxInitial=std::max(maxInitial,double(std::hypot(std::hypot(h[i].position.x-seed[i].x,h[i].position.y-seed[i].y),h[i].position.z-seed[i].z)));if(maxInitial>1e-6)throw std::runtime_error("initial wall overlap");gpu.advance(256,0,false);auto p=gpu.snapshot();if(p.size()!=20000)throw std::runtime_error("lost particles");for(auto q:p)if(!std::isfinite(q.x+q.y+q.z))throw std::runtime_error("nonfinite charge");}
 // Local receiver-to-mixer drop and rotor probes, independent of long upstream transfer.
 powder::plant_set_receiver_valve(d,true);c.plant_data=d;std::vector<powder::Grain> p;
 for(int i=0;i<12;++i)p.push_back({3.f+(i%3-1)*.006f,(i/3-1.5f)*.006f,1.325f,float(i%2)});
 // Clear points in the half-size mixer, distributed around the shaft.
 for(int i=0;i<12;++i)p.push_back({2.72f+.05f*(i%6),i<6?-.08f:.08f,1.22f,float(i%2)});
 c.count=unsigned(p.size());powder::GranularGPU local(c,p,std::vector<powder::Grain>(p.size()));
 for(int i=0;i<10000;i+=16)local.advance(16,15,false);
 auto final=local.snapshot();unsigned entered=0,inside=0;double moved=0;
 for(size_t i=0;i<final.size();++i){auto q=final[i];if(!std::isfinite(q.x+q.y+q.z))throw std::runtime_error("local nonfinite");if(i<12&&q.z<1.31)++entered;if(q.x>2.62&&q.x<3.38&&std::abs(q.y)<.227&&q.z>.938&&q.z<1.32)++inside;moved+=std::hypot(std::hypot(q.x-p[i].x,q.y-p[i].y),q.z-p[i].z);}
 if(entered!=12||inside!=24||local.angle()<=0)throw std::runtime_error("mixer inlet / confinement / rotation");
 powder::Grain worst{};float worstAngle=0;double rawSurfaceDelta=0,rawPathDelta=0;unsigned surfaceQueries=0,pathQueries=0;
 if(argc==4){
  auto raw=powder::read_plant_data(std::string(argv[3])+"/runtime.plant");powder::plant_set_receiver_valve(raw,true);c.plant_data=raw;c.plant_wall_mode=0;
  powder::GranularGPU reference(c,p,std::vector<powder::Grain>(p.size()));
  auto distance=[](auto a,auto b){return std::hypot(std::hypot(a.x-b.x,a.y-b.y),a.z-b.z);};
  std::ifstream probes(dir+"/surface_probes.bin",std::ios::binary);std::vector<powder::Grain> samples;powder::Grain q;while(probes.read(reinterpret_cast<char*>(&q),sizeof(q)))samples.push_back(q);
  for(float angle:{0.f,.35f,1.1f}){std::vector<powder::Grain> points;for(size_t i=0;i<samples.size();i+=17){auto q=samples[i];if(q.w==1){float y=q.y-d[1].y,z=q.z-d[1].z;q.y=d[1].y+std::cos(angle)*y-std::sin(angle)*z;q.z=d[1].z+std::sin(angle)*y+std::cos(angle)*z;}q.w=0;points.push_back(q);}auto a=reference.inspect_boundaries(points,angle),b=local.inspect_boundaries(points,angle);for(size_t i=0;i<a.size();++i){double delta=distance(a[i].position,b[i].position);if(delta>rawSurfaceDelta){rawSurfaceDelta=delta;worst=points[i];worstAngle=angle;}}surfaceQueries+=unsigned(points.size());}
  std::vector<powder::Grain> path;for(int j=0;j<int(d[0].w);++j){int at=int(d[0].z)+3*j;auto a=d[at],b=d[at+1];path.push_back({(a.x+b.x)/2,(a.y+b.y)/2,(a.z+b.z)/2,0});}
  auto a=reference.inspect_boundaries(path,0),b=local.inspect_boundaries(path,0);for(size_t i=0;i<a.size();++i)rawPathDelta=std::max(rawPathDelta,double(distance(a[i].position,b[i].position)));pathQueries=unsigned(path.size());if(rawPathDelta>2e-5)throw std::runtime_error("hybrid changed pipe path");
 }
 std::ofstream out(argv[2]);out<<std::setprecision(12)<<"{\"initial_count\":20000,\"initial_wall_projection_m\":"<<maxInitial<<",\"initial_advance_s\":0.00256,\"mixer_fixture_count\":24,\"inlet_passed\":"<<entered<<",\"inside_mixer\":"<<inside<<",\"local_seconds\":"<<local.time()<<",\"rotor_angle_radians\":"<<local.angle()<<",\"total_displacement_m\":"<<moved<<",\"raw_surface_queries\":"<<surfaceQueries<<",\"raw_surface_max_delta_m\":"<<rawSurfaceDelta<<",\"raw_path_queries\":"<<pathQueries<<",\"raw_path_max_delta_m\":"<<rawPathDelta<<"}";
 std::cout<<"worst reference delta at "<<worst.x<<","<<worst.y<<","<<worst.z<<" angle="<<worstAngle<<" delta="<<rawSurfaceDelta<<std::endl;
 std::cout<<"PASS full charge initial walls/finite state; local mixer inlet, confinement and rotation\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
