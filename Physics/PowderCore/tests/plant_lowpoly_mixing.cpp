#include "powder/plant.hpp"
#include "powder/plant_pressure.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <map>
#include <array>
using Clock=std::chrono::steady_clock;
double seconds(Clock::time_point t){return std::chrono::duration<double>(Clock::now()-t).count();}
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("data-directory output-csv");auto begin=Clock::now();
 auto d=powder::read_plant_data(std::string(argv[1])+"/runtime.plant");auto routes=powder::read_plant_routes(std::string(argv[1])+"/runtime.routes");
 powder::plant_set_receiver_valve(d,false);powder::plant_set_regulated_pressures(d,routes,{0,0,0},0,0,false,false,false);
 powder::GranularConfig c;c.plant=true;c.plant_threaded=true;c.kernel_optimize=true;c.plant_wall_mode=5;c.plant_data=d;c.radius=.002f;c.dt=.00001f;c.iterations=8;c.count=1;
 std::vector<powder::Grain> candidates,seed;
 // A narrow central column well inside the curved vessel, above its floor.
 // A no-overlap boundary query alone does NOT prove an exterior point is inside.
 for(int k=0;k<17;++k)for(int j=0;j<21;++j)for(int i=0;i<141;++i)candidates.push_back({2.664f+i*.0048f,-.048f+j*.0048f,1.02f+k*.0048f,0});
 {powder::GranularGPU probe(c,{{3,0,1,0}},{{0,0,0,0}});
 for(size_t offset=0;offset<candidates.size()&&seed.size()<20000;offset+=32000){
 std::vector<powder::Grain> batch(candidates.begin()+offset,candidates.begin()+std::min(offset+32000,candidates.size()));auto h=probe.inspect_boundaries(batch,0);
 for(size_t i=0;i<batch.size()&&seed.size()<20000;++i){auto a=batch[i],b=h[i].position;if(std::hypot(std::hypot(a.x-b.x,a.y-b.y),a.z-b.z)<1e-7)seed.push_back(a);}}}
 if(seed.size()!=20000)throw std::runtime_error("insufficient clear fixture positions");
 std::sort(seed.begin(),seed.end(),[](auto a,auto b){return a.x<b.x;});for(size_t i=0;i<seed.size();++i)seed[i].w=i<10000?0:1;
 c.count=unsigned(seed.size());powder::GranularGPU gpu(c,seed,std::vector<powder::Grain>(seed.size()));auto ready=Clock::now();
 std::ofstream out(argv[2]);out<<std::setprecision(10)<<"stage,sim_s,compute_wall_s,setup_s,rpm,count_A,count_B,outside,composition_variance,centroid_distance_m,crossed_midplane,rms_displacement_m,rotor_angle_rad\n";
 auto record=[&](const char* stage,float rpm){auto p=gpu.snapshot();std::map<int,std::array<int,2>> cells;double sum[2][3]{};unsigned n[2]{},outside=0,crossed=0;double square=0;
 for(size_t i=0;i<p.size();++i){auto q=p[i];if(!std::isfinite(q.x+q.y+q.z))throw std::runtime_error("nonfinite");unsigned s=unsigned(q.w);if(s>1)throw std::runtime_error("species");++n[s];sum[s][0]+=q.x;sum[s][1]+=q.y;sum[s][2]+=q.z;
 if(q.x<2.62||q.x>3.38||q.y<-.227||q.y>.227||q.z<.938||q.z>1.32)++outside;
 int ix=std::clamp(int((q.x-2.62)/.095),0,7),iy=std::clamp(int((q.y+.227)/.1135),0,3),iz=std::clamp(int((q.z-.938)/.0955),0,3);++cells[ix+8*iy+32*iz][s];
 crossed+=(s==0?q.x>=3:q.x<3);square+=(q.x-seed[i].x)*(q.x-seed[i].x)+(q.y-seed[i].y)*(q.y-seed[i].y)+(q.z-seed[i].z)*(q.z-seed[i].z);}
 double variance=0,dist=0;for(auto item:cells){auto v=item.second;double t=v[0]+v[1],a=v[0]/t-.5;variance+=t*a*a/p.size();}for(int j=0;j<3;++j){double a=sum[0][j]/n[0]-sum[1][j]/n[1];dist+=a*a;}
 out<<stage<<','<<gpu.time()<<','<<seconds(ready)<<','<<std::chrono::duration<double>(ready-begin).count()<<','<<rpm<<','<<n[0]<<','<<n[1]<<','<<outside<<','<<variance<<','<<std::sqrt(dist)<<','<<crossed<<','<<std::sqrt(square/p.size())<<','<<gpu.angle()<<'\n';out.flush();std::cout<<stage<<" sim="<<gpu.time()<<" wall="<<seconds(ready)<<" variance="<<variance<<" outside="<<outside<<std::endl;if(outside)throw std::runtime_error("fixture confinement failed; mixing conclusion invalid");};
 record("initial_direct_fixture",0);double next=.05;bool settled=false;
 while(gpu.time()<4.2&&seconds(ready)<300){float rpm=gpu.time()<.2?0:15;gpu.advance(16,rpm,false);
 if(!settled&&gpu.time()>=.2){record("settled_rotation_start",15);settled=true;}
 if(gpu.time()>=next){record("sample",rpm);next+=.05;}}
 record(gpu.time()>=4.2?"completed_one_rotation":"wall_timeout_partial",settled?15:0);
 }catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
