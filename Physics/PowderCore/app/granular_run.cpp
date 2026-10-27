#include "powder/granular.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <filesystem>
using Clock=std::chrono::steady_clock;
int main(int argc,char**argv){try{
 if(argc<4)throw std::runtime_error("count duration output [iterations dt rpm gate_time radius]");
 powder::GranularConfig c;c.count=static_cast<unsigned>(std::stoul(argv[1]));double duration=std::stod(argv[2]);if(argc>4)c.iterations=static_cast<unsigned>(std::stoul(argv[4]));if(argc>5)c.dt=std::stof(argv[5]);float rpm=argc>6?std::stof(argv[6]):30.f;double gate_time=argc>7?std::stod(argv[7]):1.e9;if(argc>8)c.radius=std::stof(argv[8]);if(argc>9)c.fast_bounds=std::stoi(argv[9])!=0;if(argc>10)c.profile=std::stoi(argv[10])!=0;
 std::filesystem::path out(argv[3]);if(!std::filesystem::create_directory(out))throw std::runtime_error("output directory exists");
 if(argc>11)c.test_mixer=std::stoi(argv[11])!=0;if(argc>12)c.friction=std::stof(argv[12]);
 powder::GranularGPU gpu(c);std::cout<<gpu.device()<<" r="<<gpu.radius()<<std::endl;
 std::ofstream csv(out/"trace.csv");csv<<"time,wall,rate,max_overlap_radius,mean_overlap_radius,mean_z,max_z,mix_index,receiver_count,count,angle\n"<<std::setprecision(10);
 {auto initial=gpu.snapshot();std::ofstream bin(out/"initial.bin",std::ios::binary);bin.write(reinterpret_cast<const char*>(initial.data()),initial.size()*sizeof(powder::Grain));if(!bin)throw std::runtime_error("initial snapshot write failed");}
 auto start=Clock::now();double next=0,nextHistory=0;bool history=argc>13&&std::stoi(argv[13])!=0;
 while(gpu.time()<duration){gpu.advance(20,gpu.time()<1?0:rpm,gpu.time()>=gate_time);if(gpu.time()<next&&gpu.time()<duration)continue;next=gpu.time()+.25;
 auto p=gpu.snapshot();float r=gpu.radius(),cell=2*r;int nx=static_cast<int>(std::ceil((c.test_mixer?1.6:1.2)/cell)),ny=static_cast<int>(std::ceil((c.test_mixer?1.:.8)/cell)),nz=static_cast<int>(std::ceil((c.test_mixer?1.6:1.2)/cell));std::vector<int> heads(nx*ny*nz,-1),links(p.size());
 auto coord=[&](const powder::Grain& a){return std::array<int,3>{int(std::floor((a.x+(c.test_mixer?.8f:.6f))/cell)),int(std::floor((a.y+(c.test_mixer?.5f:.4f))/cell)),int(std::floor((a.z+(c.test_mixer?.5f:.65f))/cell))};};
 for(unsigned i=0;i<p.size();i++){auto q=coord(p[i]);if(q[0]<0||q[0]>=nx||q[1]<0||q[1]>=ny||q[2]<0||q[2]>=nz)throw std::runtime_error("escaped domain");int id=(q[2]*ny+q[1])*nx+q[0];links[i]=heads[id];heads[id]=static_cast<int>(i);}
 double maximum=0,sum=0,zsum=0,zmax=-1;unsigned contacts=0,receiver=0;std::array<std::array<unsigned,2>,64> bins{};unsigned groupA=0,mixerCount=0;
 for(unsigned i=0;i<p.size();i++){auto a=p[i];if(!std::isfinite(a.x)||!std::isfinite(a.y)||!std::isfinite(a.z))throw std::runtime_error("nonfinite");zsum+=a.z;zmax=std::max(zmax,double(a.z));if(!c.test_mixer&&a.z<-.25)receiver++;if(c.test_mixer||a.z>=0){mixerCount++;if(a.w==0)groupA++;
 int bx=std::clamp(int((a.x+(c.test_mixer?.75f:.45f))/(c.test_mixer?1.5f:.9f)*4),0,3),by=std::clamp(int((a.y+(c.test_mixer?.45f:.25f))/(c.test_mixer?.9f:.5f)*4),0,3),bz=std::clamp(int((a.z+(c.test_mixer?.325f:0))/(c.test_mixer?.65f:.4f)*4),0,3);bins[(bz*4+by)*4+bx][a.w==0?0:1]++;}
 auto q=coord(a);for(int zz=-1;zz<=1;zz++)for(int yy=-1;yy<=1;yy++)for(int xx=-1;xx<=1;xx++){int x=q[0]+xx,y=q[1]+yy,z=q[2]+zz;if(x<0||x>=nx||y<0||y>=ny||z<0||z>=nz)continue;for(int j=heads[(z*ny+y)*nx+x];j>=0;j=links[j])if(unsigned(j)>i){double dx=a.x-p[j].x,dy=a.y-p[j].y,dz=a.z-p[j].z;double overlap=2*r-std::sqrt(dx*dx+dy*dy+dz*dz);if(overlap>0){maximum=std::max(maximum,overlap/r);sum+=overlap/r;contacts++;}}}
 }
 double fraction=mixerCount?double(groupA)/mixerCount:0,variance=0;for(auto b:bins){double n=b[0]+b[1];if(n)variance+=n/mixerCount*std::pow(b[0]/n-fraction,2);}double mix=(mixerCount&&groupA&&groupA<mixerCount)?1-variance/(fraction*(1-fraction)):-1;double wall=std::chrono::duration<double>(Clock::now()-start).count();
 csv<<gpu.time()<<','<<wall<<','<<gpu.time()/wall<<','<<maximum<<','<<(contacts?sum/contacts:0)<<','<<zsum/p.size()<<','<<zmax<<','<<mix<<','<<receiver<<','<<p.size()<<','<<gpu.angle()<<'\n';csv.flush();std::cout<<gpu.time()<<" rate "<<gpu.time()/wall<<" overlap/r "<<maximum<<" mix "<<mix<<" receiver "<<receiver<<std::endl;
 std::ofstream snapshot(out/"final.bin",std::ios::binary);snapshot.write(reinterpret_cast<const char*>(p.data()),p.size()*sizeof(powder::Grain));
 if(history&&gpu.time()>=nextHistory){std::ofstream h(out/("frame_"+std::to_string(gpu.time())+"_"+std::to_string(gpu.angle())+".bin"),std::ios::binary);h.write(reinterpret_cast<const char*>(p.data()),p.size()*sizeof(powder::Grain));nextHistory=gpu.time()+1;}

 }
 auto timing=gpu.timing();std::ofstream profile(out/"timing.json");profile<<std::setprecision(12)<<"{\"profiling_enabled\":"<<(c.profile?"true":"false")<<",\"advance_wall_s\":"<<timing.advance_wall<<",\"snapshot_wall_s\":"<<timing.snapshot_wall<<",\"grid_clear_gpu_s\":"<<timing.gpu_seconds[0]<<",\"grid_build_gpu_s\":"<<timing.gpu_seconds[1]<<",\"predict_gpu_s\":"<<timing.gpu_seconds[2]<<",\"contact_gpu_s\":"<<timing.gpu_seconds[3]<<",\"velocity_gpu_s\":"<<timing.gpu_seconds[4]<<"}";
 return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
