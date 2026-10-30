#include "powder/plant.hpp"
#include <iostream>
#include <sstream>
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("data directory");std::string root=argv[1];auto d=powder::read_plant_data(root+"/runtime.plant");if(d[10].x!=1||d[0].x!=-1)throw std::runtime_error("not OBB mode");
 auto routes=powder::read_plant_routes(root+"/runtime.routes");powder::plant_set_manual_valves(d,routes,1000,7,7,true,true);powder::plant_set_receiver_valve(d,true);
 powder::GranularConfig c;c.plant=true;c.plant_threaded=true;c.kernel_optimize=true;c.plant_data=d;c.count=1;c.radius=.00125f;c.dt=.00001f;c.iterations=8;powder::GranularGPU gpu(c,{{-1.2f,1.4f,3.f,0}},{{0,0,0,0}});
 std::vector<powder::Grain> probes;for(const auto&r:routes)for(int k:r.cells){auto a=d[k],b=d[k+1];for(int j=0;j<=4;++j){float t=j/4.f;probes.push_back({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,0});}}
 auto result=gpu.inspect_boundaries(probes,0,false,false);float maxMove=0;for(size_t i=0;i<probes.size();++i){auto a=probes[i],b=result[i].position;float move=std::hypot(std::hypot(a.x-b.x,a.y-b.y),a.z-b.z);maxMove=std::max(move,maxMove);if(move>.0001f){std::cerr<<"blocked centreline "<<a.x<<","<<a.y<<","<<a.z<<" correction="<<move<<std::endl;return 1;}}
 // An isolated pair of overlapping rotated interior boxes and one solid slab.
 auto isolated=d;isolated[0].y=-1;isolated[5].y=0;isolated[9]={float(isolated.size()),2,float(isolated.size()+8),1};float q=std::sqrt(.5f);
 for(float x:{0.f, .8f}){isolated.push_back({x*q,x*q,0,0});isolated.push_back({q,q,0,.5f});isolated.push_back({-q,q,0,.2f});isolated.push_back({0,0,1,.2f});}
 isolated.push_back({0,0,0,0});isolated.push_back({q,q,0,.01f});isolated.push_back({-q,q,0,.19f});isolated.push_back({0,0,1,.19f});c.plant_data=isolated;powder::GranularGPU simple(c,{{.1f*q,.1f*q,0,0}},{{0,0,0,0}});
 std::vector<powder::Grain> points{{.4f*q,.4f*q,0,0},{-.205f*q,.205f*q,0,0},{.0105f*q,.0105f*q,0,0}};auto hits=simple.inspect_boundaries(points,0,false,false);
 if(std::hypot(hits[0].position.x-points[0].x,hits[0].position.y-points[0].y)>1e-6)throw std::runtime_error("union joint blocked");
 if(std::abs((-hits[1].position.x+hits[1].position.y)*q-(.2f-c.radius))>2e-6)throw std::runtime_error("rotated interior wall failed");
 if(std::abs((hits[2].position.x+hits[2].position.y)*q-(.01f+c.radius))>2e-6)throw std::runtime_error("rotated solid contact failed");
 std::cout<<"PASS OBB union joint, rotated inner wall, solid slab; "<<probes.size()<<" route-centre probes, max correction="<<maxMove<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
