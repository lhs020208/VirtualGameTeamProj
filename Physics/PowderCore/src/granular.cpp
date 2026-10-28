#include <cstring>
#define CL_TARGET_OPENCL_VERSION 120
#include "powder/granular.hpp"
#include <CL/cl.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <set>
#include <map>
#include <algorithm>
#include <stdexcept>
#include <ostream>
#include <iomanip>
#include <sstream>
namespace powder {
namespace {
using Clock=std::chrono::steady_clock;using V=std::array<double,4>;
double elapsed(Clock::time_point t){return std::chrono::duration<double>(Clock::now()-t).count();}
void require(bool b,const char* s){if(!b)throw std::invalid_argument(s);}
void check(cl_int code,const char* op){if(code)throw std::runtime_error(std::string(op)+" OpenCL error "+std::to_string(code));}
#include "granular_mx301_cpu.inc"
const char* source=
#include "granular_mx301_cl.inc"

#include "granular_kernels.inc"
;
#define GPU_API(X) X(clGetPlatformIDs) X(clGetDeviceIDs) X(clGetDeviceInfo) X(clCreateContext) X(clCreateCommandQueue) X(clCreateProgramWithSource) X(clBuildProgram) X(clGetProgramBuildInfo) X(clCreateKernel) X(clCreateBuffer) X(clSetKernelArg) X(clEnqueueWriteBuffer) X(clEnqueueReadBuffer) X(clEnqueueNDRangeKernel) X(clFinish) X(clFlush) X(clWaitForEvents) X(clGetEventInfo) X(clGetEventProfilingInfo) X(clReleaseEvent) X(clReleaseMemObject) X(clReleaseKernel) X(clReleaseProgram) X(clReleaseCommandQueue) X(clReleaseContext)
struct Api {
 HMODULE module=nullptr;
 #define DECLARE(name) decltype(&::name) name=nullptr;
 GPU_API(DECLARE)
 #undef DECLARE
 Api(){module=LoadLibraryExW(L"OpenCL.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!module)throw std::runtime_error("system OpenCL runtime unavailable");
 #define LOAD(name) name=reinterpret_cast<decltype(name)>(GetProcAddress(module,#name));if(!name){FreeLibrary(module);module=nullptr;throw std::runtime_error("missing OpenCL entry " #name);}
 GPU_API(LOAD)
 #undef LOAD
 }
 ~Api(){if(module)FreeLibrary(module);}
};
}
void validate_granular_initial(GranularConfig c,const std::vector<Grain>& p){
 require(c.count==p.size()&&c.count>0&&c.count<=1000000,"external initial count mismatch");
 const float maxr=c.binary_materials?float(std::max(c.materials[0].radius_m,c.materials[1].radius_m)):c.radius;
 if(c.binary_materials)for(auto& m:c.materials)m.validate();
 const float rmax=maxr;require(std::isfinite(rmax)&&rmax>.001f&&rmax<.03f,"external radius outside GPU range");
 std::map<std::array<int,3>,std::vector<std::size_t>> grid;const double pitch=2*double(maxr),tol=1.e-6;
 for(std::size_t i=0;i<p.size();++i){auto a=p[i];float x=a.x,y=a.y,z=a.z;
  const float r=c.binary_materials?float(c.materials[a.w==0?0:1].radius_m):c.radius;
  require(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z)&&(a.w==0||a.w==1),"invalid external position or group");
  if(c.test_mixer){
   for(int j=0;j<133;++j){auto plane=mxplanes[j];require(plane[0]*x+plane[1]*y+plane[2]*z-plane[3]<=-r+tol,"initial particle outside MX301 vessel");}
   for(auto& part:mxparts){double d=-1.e9;for(int j=int(part[0]);j<int(part[0]+part[1]);++j){auto plane=mxplanes[j];d=std::max(d,double(plane[0]*x+plane[1]*y+plane[2]*z-plane[3]));}require(d>=r-tol,"initial particle overlaps MX301 rotor");}
  }else{
   require(std::abs(x)<=.45-r+tol&&std::abs(y)<=.25-r+tol&&z>=r-tol&&z<=.4-r+tol,"initial particle outside reference vessel");
   require(std::hypot(y,z-.21f)>=.025f+r-tol,"initial particle overlaps shaft");
   for(unsigned k=0;k<6;k++){float angle=k*1.04719755f,co=std::cos(angle),si=std::sin(angle);float lx=x-(-.375f+k*.15f),ly=co*y+si*(z-.21f)-.095f,lz=-si*y+co*(z-.21f);float beta=k%2?-.4f:.4f;float tx=std::cos(beta)*lx+std::sin(beta)*ly,ty=-std::sin(beta)*lx+std::cos(beta)*ly;float dx=std::max(0.f,std::abs(tx)-.045f),dy=std::max(0.f,std::abs(ty)-.07f),dz=std::max(0.f,std::abs(lz)-.012f);require(dx*dx+dy*dy+dz*dz>=(r-tol)*(r-tol),"initial particle overlaps paddle");}
  }
  std::array<int,3> key{int(std::floor(x/pitch)),int(std::floor(y/pitch)),int(std::floor(z/pitch))};
  for(int dz=-1;dz<=1;dz++)for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){auto it=grid.find({key[0]+dx,key[1]+dy,key[2]+dz});if(it==grid.end())continue;for(auto j:it->second){double ax=x-p[j].x,ay=y-p[j].y,az=z-p[j].z;require(ax*ax+ay*ay+az*az>=std::pow(double(r)+(c.binary_materials?c.materials[p[j].w==0?0:1].radius_m:c.radius)-tol,2),"initial particles overlap (including between emitters)");}}
  grid[key].push_back(i);
 }
}
struct GranularGPU::Impl {
 Api api;cl_device_id device=nullptr;cl_context context=nullptr;cl_command_queue queue=nullptr;cl_program program=nullptr;
 std::array<std::array<std::vector<unsigned char>,24>,14> argument_cache;
 cl_event read_event=nullptr;std::vector<Grain> read_buffer;
 std::array<cl_kernel,14> kernels{};std::array<cl_mem,19> buffers{};
 GranularConfig c;int nx=0,ny=0,nz=0,cells=0;float cell=0,angle=0;double time=0;std::string name;bool poisoned=false;GranularTiming timing;std::vector<std::pair<cl_event,unsigned>> events;
 Impl(GranularConfig cfg,const std::vector<Grain>* external=nullptr,const std::vector<Grain>* velocity=nullptr):c(cfg){try{
 require(!c.rotation||c.binary_materials,"rotation requires binary materials");require(std::isfinite(c.rolling_friction)&&c.rolling_friction>=0&&c.rolling_friction<=1,"invalid rolling friction");
 require(!c.boundary_candidates||c.test_mixer,"boundary candidates currently require MX301 test geometry");
 require(!c.candidate_reuse||c.candidates,"reuse requires candidates");
 require(!c.candidate_audit||(c.candidates&&c.count<=4096),"all-pairs audit requires candidates and <=4096 particles");
 require(std::isfinite(c.candidate_skin_ratio)&&c.candidate_skin_ratio>=.01f&&c.candidate_skin_ratio<=2.f,"skin ratio must be .01..2");
 require(c.candidate_capacity>=1&&c.candidate_capacity<=256,"candidate capacity must be 1..256");
 require(!c.profile_split||c.profile,"split diagnostic requires profiling");
 require(c.count>=1&&c.count<=1000000&&c.iterations>=2&&c.iterations<=32,"granular count/iterations outside contract");
 if(c.binary_materials){for(auto& m:c.materials)m.validate();require(std::isfinite(c.cross_friction)&&c.cross_friction>=0&&c.cross_friction<=1,"invalid cross friction");c.radius=float(std::max(c.materials[0].radius_m,c.materials[1].radius_m));}
 if(c.radius==0)c.radius=static_cast<float>(std::cbrt(.054/(c.count*4.188790204786391)));
 require(std::isfinite(c.radius)&&c.radius>.001f&&c.radius<.03f&&std::isfinite(c.dt)&&c.dt>0&&c.dt<=.001f&&std::isfinite(c.friction)&&c.friction>=0&&c.friction<=1,"invalid granular configuration");
 cl_uint count=0;check(api.clGetPlatformIDs(0,nullptr,&count),"platform count");std::vector<cl_platform_id> platforms(count);check(api.clGetPlatformIDs(count,platforms.data(),nullptr),"platforms");
 for(auto platform:platforms){cl_uint n=0;if(api.clGetDeviceIDs(platform,CL_DEVICE_TYPE_GPU,0,nullptr,&n)||!n)continue;std::vector<cl_device_id> ids(n);check(api.clGetDeviceIDs(platform,CL_DEVICE_TYPE_GPU,n,ids.data(),nullptr),"devices");device=ids[0];break;}require(device!=nullptr,"no OpenCL GPU");
 char text[256]{};check(api.clGetDeviceInfo(device,CL_DEVICE_NAME,sizeof(text),text,nullptr),"device name");name=text;
 cl_int err=0;context=api.clCreateContext(nullptr,1,&device,nullptr,nullptr,&err);check(err,"context");queue=api.clCreateCommandQueue(context,device,c.profile?CL_QUEUE_PROFILING_ENABLE:0,&err);check(err,"queue");
 program=api.clCreateProgramWithSource(context,1,&source,nullptr,&err);check(err,"program");std::string options=std::string("-cl-std=CL1.2 -DGRANULAR_BOUNDS_FAST=")+(c.fast_bounds?"1":"0")+" -DGRANULAR_MX301="+(c.test_mixer?"1":"0")+" -DGRANULAR_PROFILE_SPLIT="+(c.profile_split?"1":"0")+" -DGRANULAR_CANDIDATES="+(c.candidates?"1":"0")+" -DGRANULAR_REUSE="+(c.candidate_reuse?"1":"0")+" -DGRANULAR_BOUNDARY_CANDIDATES="+(c.boundary_candidates?"1":"0")+" -DGRANULAR_KERNEL_OPTIMIZE="+(c.kernel_optimize?"1":"0");std::ostringstream defines;defines<<std::scientific<<std::setprecision(9);
 defines<<" -DGRANULAR_ROTATION="<<(c.rotation?1:0)<<" -DMAT_ROLL="<<c.rolling_friction<<"f";
 defines<<" -DGRANULAR_BINARY="<<(c.binary_materials?1:0);
 for(int g=0;g<2;++g){const auto& m=c.materials[g];defines<<" -DMAT_R"<<g<<"="<<float(m.radius_m)<<"f -DMAT_M"<<g<<"="<<float(m.mass_kg())<<"f -DMAT_MU"<<g<<"="<<float(m.particle_friction)<<"f -DMAT_W"<<g<<"="<<float(m.wall_friction)<<"f";}
 defines<<" -DMAT_CROSS="<<c.cross_friction<<"f";options+=defines.str();
 err=api.clBuildProgram(program,1,&device,options.c_str(),nullptr,nullptr);if(err){std::size_t size=0;api.clGetProgramBuildInfo(program,device,CL_PROGRAM_BUILD_LOG,0,nullptr,&size);std::string log(size,' ');api.clGetProgramBuildInfo(program,device,CL_PROGRAM_BUILD_LOG,size,log.data(),nullptr);throw std::runtime_error(log);}
 const char* names[]={"cleargrid","grid","predict","solve","velocity","project_boundary","probe_boundary"};for(unsigned k=0;k<(c.profile_split?7u:5u);k++){kernels[k]=api.clCreateKernel(program,names[k],&err);check(err,"kernel");}
 if(c.candidates){kernels[8]=api.clCreateKernel(program,"build_candidates",&err);check(err,"candidate kernel");}
 if(c.candidate_reuse){kernels[9]=api.clCreateKernel(program,"reuse_begin",&err);check(err,"reuse begin kernel");kernels[10]=api.clCreateKernel(program,"reuse_check",&err);check(err,"reuse check kernel");}
 if(c.candidate_audit){kernels[11]=api.clCreateKernel(program,"audit_candidates",&err);check(err,"candidate audit kernel");}
 cell=2*c.radius+(c.candidate_reuse?c.radius*c.candidate_skin_ratio:0);nx=static_cast<int>(std::ceil((c.test_mixer?1.6:(c.fast_bounds?.92:1.2))/cell));ny=static_cast<int>(std::ceil((c.test_mixer?1.0:(c.fast_bounds?.52:.8))/cell));nz=static_cast<int>(std::ceil((c.test_mixer?1.0:(c.fast_bounds?1.02:1.2))/cell));cells=nx*ny*nz;
 for(unsigned i=0;i<(c.profile_split?10u:8u);i++){std::size_t bytes=(i<4||i>=7)?c.count*sizeof(Grain):i==4?cells*sizeof(int):i==5?c.count*sizeof(int):sizeof(int);buffers[i]=api.clCreateBuffer(context,CL_MEM_READ_WRITE,bytes,nullptr,&err);check(err,"buffer");}
 if(c.candidates){for(unsigned i=10;i<13;++i){std::size_t bytes=i==10?std::size_t(c.count)*c.candidate_capacity*sizeof(int):i==11?std::size_t(c.count)*sizeof(int):2*sizeof(cl_uint);buffers[i]=api.clCreateBuffer(context,CL_MEM_READ_WRITE,bytes,nullptr,&err);check(err,"candidate buffer allocation");}}
 if(c.candidate_reuse){buffers[13]=api.clCreateBuffer(context,CL_MEM_READ_WRITE,c.count*sizeof(Grain),nullptr,&err);check(err,"reference positions");buffers[14]=api.clCreateBuffer(context,CL_MEM_READ_WRITE,6*sizeof(cl_uint),nullptr,&err);check(err,"reuse control");arg(0,2,buffers[14]);arg(1,9,buffers[14]);}
 if(c.boundary_candidates){kernels[12]=api.clCreateKernel(program,"init_boundary_groups",&err);check(err,"boundary group kernel");buffers[15]=api.clCreateBuffer(context,CL_MEM_READ_WRITE,44*sizeof(Grain),nullptr,&err);check(err,"boundary group buffer");arg(12,0,buffers[15]);run(12,22);}
 arg(3,c.candidates?17:14,buffers[15]);arg(3,c.candidates?18:15,buffers[6]);arg(4,c.profile_split?14:12,buffers[15]);if(c.profile_split){arg(5,5,buffers[15]);arg(6,7,buffers[15]);}
 if(c.rotation){for(unsigned b=16;b<19;++b){buffers[b]=api.clCreateBuffer(context,CL_MEM_READ_WRITE,c.count*sizeof(Grain),nullptr,&err);check(err,"angular buffer");}}
 if(external){validate_granular_initial(c,*external);require(velocity!=nullptr,"missing external velocities");set(*external,*velocity);return;}
 std::vector<Grain> initial;initial.reserve(c.count);float pitch=2.04f*c.radius;
 if(!c.test_mixer)for(float z=c.radius;z<.4f-c.radius&&initial.size()<c.count;z+=pitch)for(float y=-.25f+c.radius;y<.25f-c.radius&&initial.size()<c.count;y+=pitch)for(float x=-.45f+c.radius;x<.45f-c.radius&&initial.size()<c.count;x+=pitch){
  bool hit=std::hypot(y,z-.21f)<.025f+c.radius;
  for(unsigned k=0;k<6;k++){float a=k*1.04719755f,co=std::cos(a),si=std::sin(a);float lx=x-(-.375f+k*.15f),ly=co*y+si*(z-.21f)-.095f,lz=-si*y+co*(z-.21f);float beta=k%2?-.4f:.4f;float tx=std::cos(beta)*lx+std::sin(beta)*ly,ty=-std::sin(beta)*lx+std::cos(beta)*ly;float dx=std::max(0.f,std::abs(tx)-.045f),dy=std::max(0.f,std::abs(ty)-.07f),dz=std::max(0.f,std::abs(lz)-.012f);hit=hit||dx*dx+dy*dy+dz*dz<c.radius*c.radius;}
  if(!hit)initial.push_back({x,y,z,x<0?0.f:1.f});
 }
 if(c.test_mixer){for(float z=-.325f+c.radius;z<.325f-c.radius&&initial.size()<c.count;z+=pitch)for(float y=-.45f+c.radius;y<.45f-c.radius&&initial.size()<c.count;y+=pitch)for(float x=-.75f+c.radius;x<.75f-c.radius&&initial.size()<c.count;x+=pitch){
  bool hit=false;for(int j=0;j<133;j++){auto a=mxplanes[j];if(a[0]*x+a[1]*y+a[2]*z-a[3]>-c.radius){hit=true;break;}}
  if(!hit)for(auto& part:mxparts){float distance=-1.e9f;for(int j=int(part[0]);j<int(part[0]+part[1]);j++){auto a=mxplanes[j];distance=std::max(distance,a[0]*x+a[1]*y+a[2]*z-a[3]);}if(distance<c.radius){hit=true;break;}}
  if(!hit)initial.push_back({x,y,z,x<0?0.f:1.f});
 }}
 require(initial.size()==c.count,"initial charge does not fit vessel");
 // MX301 charge is generated bottom-to-top: red lower half, blue upper half.
 // Keep positions and equal material counts; only the interface layer may share both groups.
 if(c.test_mixer)for(std::size_t i=0;i<initial.size();++i)initial[i].w=i<initial.size()/2?0.f:1.f;
 std::vector<Grain> v(c.count);set(initial,v);
 }catch(...){release();throw;}}
 void drain_read(){if(read_event){api.clWaitForEvents(1,&read_event);api.clReleaseEvent(read_event);read_event=nullptr;}}
 void release(){drain_read();for(auto e:events)if(e.first)api.clReleaseEvent(e.first);events.clear();for(auto k:kernels)if(k)api.clReleaseKernel(k);for(auto b:buffers)if(b)api.clReleaseMemObject(b);if(program)api.clReleaseProgram(program);if(queue)api.clReleaseCommandQueue(queue);if(context)api.clReleaseContext(context);}
 ~Impl(){release();}
 template<class T>void arg(unsigned k,unsigned i,const T& v){
 auto& cached=argument_cache[k][i];
 if(c.kernel_optimize&&cached.size()==sizeof(v)&&std::memcmp(cached.data(),&v,sizeof(v))==0){++timing.argument_cache_hits;return;}
 check(api.clSetKernelArg(kernels[k],i,sizeof(v),&v),"arg");++timing.argument_calls;
 if(c.kernel_optimize){cached.resize(sizeof(v));std::memcpy(cached.data(),&v,sizeof(v));}
 }
 void run(unsigned k,std::size_t n){std::size_t local=128,global=(n+127)/128*128;cl_event event=nullptr;check(api.clEnqueueNDRangeKernel(queue,kernels[k],1,nullptr,&global,&local,0,nullptr,c.profile?&event:nullptr),"dispatch");if(event)events.push_back({event,k});}
 void set(const std::vector<Grain>& state,const std::vector<Grain>& vel){require(state.size()==c.count&&vel.size()==c.count,"state count mismatch");for(unsigned i=0;i<c.count;i++)for(float v:{state[i].x,state[i].y,state[i].z,state[i].w,vel[i].x,vel[i].y,vel[i].z})require(std::isfinite(v),"nonfinite input state");
 for(const auto& a:state)require(a.w==0||a.w==1,"invalid group ID");
 drain_read();check(api.clEnqueueWriteBuffer(queue,buffers[0],CL_TRUE,0,state.size()*sizeof(Grain),state.data(),0,nullptr,nullptr),"state write");check(api.clEnqueueWriteBuffer(queue,buffers[2],CL_TRUE,0,vel.size()*sizeof(Grain),vel.data(),0,nullptr,nullptr),"velocity write");time=0;angle=0;poisoned=false;for(auto e:events)if(e.first)api.clReleaseEvent(e.first);events.clear();timing={};timing.profiling_enabled=c.profile;timing.split_boundaries=c.profile_split;timing.candidates_enabled=c.candidates;timing.candidate_capacity=c.candidates?c.candidate_capacity:0;timing.boundary_candidates_enabled=c.boundary_candidates;timing.kernel_optimize_enabled=c.kernel_optimize;timing.candidate_reuse_enabled=c.candidate_reuse;timing.candidate_audit_enabled=c.candidate_audit;timing.candidate_skin_m=c.candidate_reuse?c.radius*c.candidate_skin_ratio:0;
 if(c.rotation){std::vector<Grain> zeros(c.count);for(unsigned b=16;b<19;++b)check(api.clEnqueueWriteBuffer(queue,buffers[b],CL_TRUE,0,zeros.size()*sizeof(Grain),zeros.data(),0,nullptr,nullptr),"reset angular state");}
 if(c.candidate_reuse){cl_uint control[6]{};check(api.clEnqueueWriteBuffer(queue,buffers[14],CL_TRUE,0,sizeof(control),control,0,nullptr,nullptr),"reset reuse state");}}
 void collect(){auto begin=Clock::now();for(auto& e:events){cl_ulong start=0,end=0;check(api.clGetEventProfilingInfo(e.first,CL_PROFILING_COMMAND_START,sizeof(start),&start,nullptr),"profile start");check(api.clGetEventProfilingInfo(e.first,CL_PROFILING_COMMAND_END,sizeof(end),&end,nullptr),"profile end");require(end>=start,"invalid GPU timestamps");timing.gpu_seconds[e.second]+=(end-start)*1.e-9;++timing.calls[e.second];api.clReleaseEvent(e.first);e.first=nullptr;}events.clear();if(c.profile)timing.profile_collect_wall+=elapsed(begin);}
 void build_candidates(){int n=static_cast<int>(c.count),capacity=static_cast<int>(c.candidate_capacity);
  arg(8,0,buffers[0]);arg(8,1,buffers[4]);arg(8,2,buffers[5]);arg(8,3,n);arg(8,4,c.radius);arg(8,5,cell);arg(8,6,nx);arg(8,7,ny);arg(8,8,nz);arg(8,9,buffers[10]);arg(8,10,buffers[11]);arg(8,11,capacity);arg(8,12,buffers[12]);if(c.candidate_reuse){float skin=c.radius*c.candidate_skin_ratio;arg(8,13,buffers[13]);arg(8,14,buffers[14]);arg(8,15,skin);}run(8,c.count);if(!c.candidate_reuse)++timing.candidate_builds;
 }
 void check_reuse(){int n=static_cast<int>(c.count);float skin=c.radius*c.candidate_skin_ratio;
 arg(9,0,buffers[14]);run(9,1);arg(10,0,buffers[0]);arg(10,1,buffers[13]);arg(10,2,n);arg(10,3,skin);arg(10,4,buffers[14]);arg(10,5,cell);arg(10,6,nx);arg(10,7,ny);arg(10,8,nz);arg(10,9,buffers[6]);run(10,c.count);
 }
 void audit_candidates(){int n=static_cast<int>(c.count),capacity=static_cast<int>(c.candidate_capacity);
 arg(11,0,buffers[0]);arg(11,1,n);arg(11,2,c.radius);arg(11,3,buffers[10]);arg(11,4,buffers[11]);arg(11,5,capacity);arg(11,6,buffers[4]);arg(11,7,buffers[5]);arg(11,8,cell);arg(11,9,nx);arg(11,10,ny);arg(11,11,nz);arg(11,12,buffers[6]);run(11,c.count);
 }
 void advance(unsigned steps,float rpm,bool gate){auto begin=Clock::now();require(!poisoned,"reset required after rejected GPU batch");require(steps>0&&steps<=1000&&std::isfinite(rpm)&&std::abs(rpm)<=60,"invalid granular control");int zero=0;check(api.clEnqueueWriteBuffer(queue,buffers[6],CL_FALSE,0,sizeof(int),&zero,0,nullptr,nullptr),"clear error");cl_ulong zeroOverflow=0;if(c.candidates)check(api.clEnqueueWriteBuffer(queue,buffers[12],CL_FALSE,0,sizeof(zeroOverflow),&zeroOverflow,0,nullptr,nullptr),"clear candidate overflow");if(c.candidate_reuse)check(api.clEnqueueWriteBuffer(queue,buffers[14],CL_FALSE,3*sizeof(cl_uint),2*sizeof(cl_uint),&zeroOverflow,0,nullptr,nullptr),"clear reuse counters");int n=static_cast<int>(c.count),g=gate?1:0;float omega=rpm*.104719755f;
 for(unsigned s=0;s<steps;s++){
  angle+=omega*c.dt;
  arg(2,0,buffers[0]);arg(2,1,buffers[1]);arg(2,2,buffers[2]);arg(2,3,n);arg(2,4,c.dt);arg(2,5,c.radius);arg(2,6,buffers[6]);arg(2,7,buffers[7]);if(c.rotation){arg(2,8,buffers[16]);arg(2,9,buffers[18]);}run(2,c.count);
  for(unsigned it=0;it<c.iterations;it++){
   if(c.candidate_reuse)check_reuse();
   arg(0,0,buffers[4]);arg(0,1,cells);run(0,cells);
   arg(1,0,buffers[0]);arg(1,1,buffers[4]);arg(1,2,buffers[5]);arg(1,3,n);arg(1,4,cell);arg(1,5,nx);arg(1,6,ny);arg(1,7,nz);arg(1,8,buffers[6]);run(1,c.count);
   if(c.candidates){build_candidates();int capacity=static_cast<int>(c.candidate_capacity);arg(3,14,buffers[10]);arg(3,15,buffers[11]);arg(3,16,capacity);}
   if(c.candidate_audit)audit_candidates();
   arg(3,0,buffers[0]);arg(3,1,buffers[1]);arg(3,2,buffers[3]);arg(3,3,buffers[4]);arg(3,4,buffers[5]);arg(3,5,n);arg(3,6,c.radius);arg(3,7,cell);arg(3,8,nx);arg(3,9,ny);arg(3,10,nz);arg(3,11,angle);arg(3,12,g);arg(3,13,c.friction);if(c.rotation){unsigned a=c.candidates?19:16;arg(3,a,buffers[16]);arg(3,a+1,buffers[17]);}run(3,c.count);
   if(c.profile_split){arg(5,0,buffers[3]);arg(5,1,n);arg(5,2,c.radius);arg(5,3,angle);arg(5,4,g);run(5,c.count);}
   std::swap(buffers[0],buffers[3]);if(c.rotation)std::swap(buffers[16],buffers[17]);
  }
  if(c.profile_split){arg(6,0,buffers[0]);arg(6,1,n);arg(6,2,c.radius);arg(6,3,angle);arg(6,4,g);arg(6,5,buffers[8]);arg(6,6,buffers[9]);run(6,c.count);arg(4,12,buffers[8]);arg(4,13,buffers[9]);}
  arg(4,0,buffers[0]);arg(4,1,buffers[1]);arg(4,2,buffers[2]);arg(4,3,n);arg(4,4,c.dt);arg(4,5,c.radius);arg(4,6,angle);arg(4,7,omega);arg(4,8,g);arg(4,9,c.friction);arg(4,10,buffers[6]);arg(4,11,buffers[7]);if(c.rotation){unsigned a=c.profile_split?15:13;arg(4,a,buffers[16]);arg(4,a+1,buffers[18]);}run(4,c.count);
 }
 timing.submit_wall+=elapsed(begin);auto guardBegin=Clock::now();
 int error=0;check(api.clEnqueueReadBuffer(queue,buffers[6],CL_TRUE,0,sizeof(int),&error,0,nullptr,nullptr),"guard");timing.guard_wait_wall+=elapsed(guardBegin);
 if(c.candidates){cl_ulong overflow=0;check(api.clEnqueueReadBuffer(queue,buffers[12],CL_TRUE,0,sizeof(overflow),&overflow,0,nullptr,nullptr),"candidate fallback count");timing.candidate_fallback_particle_iterations+=overflow;}
 if(c.candidate_reuse){cl_uint counts[2]{};check(api.clEnqueueReadBuffer(queue,buffers[14],CL_TRUE,3*sizeof(cl_uint),sizeof(counts),counts,0,nullptr,nullptr),"reuse counters");timing.candidate_builds+=counts[0];timing.reuse_checks+=counts[1];timing.candidate_reuses+=counts[1]-counts[0];}
 collect();timing.advance_wall+=elapsed(begin);
 if(error)throw std::runtime_error("granular safety rejection bits="+std::to_string(error)+" (1=domain,2=step displacement,4=nonfinite,8=missing candidate,16=excessive mass-weighted correction); reset required");time+=steps*c.dt;
 }
};
GranularGPU::GranularGPU(GranularConfig c):p(std::make_unique<Impl>(c)){}
GranularGPU::GranularGPU(GranularConfig c,const std::vector<Grain>& a,const std::vector<Grain>& b):p(std::make_unique<Impl>(c,&a,&b)){}
GranularGPU::~GranularGPU()=default;
void GranularGPU::advance(unsigned s,float rpm,bool gate){try{p->advance(s,rpm,gate);}catch(...){p->poisoned=true;throw;}}
std::vector<Grain> GranularGPU::snapshot(){auto begin=Clock::now();std::vector<Grain> result(p->c.count);cl_event event=nullptr;check(p->api.clEnqueueReadBuffer(p->queue,p->buffers[0],CL_TRUE,0,result.size()*sizeof(Grain),result.data(),0,nullptr,p->c.profile?&event:nullptr),"snapshot");if(event){p->events.push_back({event,7});p->collect();}p->timing.snapshot_wall+=elapsed(begin);return result;}
std::vector<Grain> GranularGPU::velocity_snapshot(){require(!p->poisoned,"invalid solver");std::vector<Grain> v(p->c.count);check(p->api.clEnqueueReadBuffer(p->queue,p->buffers[2],CL_TRUE,0,v.size()*sizeof(Grain),v.data(),0,nullptr,nullptr),"velocity snapshot");return v;}
 std::vector<Grain> GranularGPU::angular_velocity_snapshot(){require(p->c.rotation&&!p->poisoned,"angular state unavailable");std::vector<Grain> v(p->c.count);check(p->api.clEnqueueReadBuffer(p->queue,p->buffers[18],CL_TRUE,0,v.size()*sizeof(Grain),v.data(),0,nullptr,nullptr),"angular snapshot");return v;}
 void GranularGPU::request_snapshot(){
 require(!p->poisoned&&!p->read_event,"snapshot pending or solver invalid");auto begin=Clock::now();p->read_buffer.resize(p->c.count);
 check(p->api.clEnqueueReadBuffer(p->queue,p->buffers[0],CL_FALSE,0,p->read_buffer.size()*sizeof(Grain),p->read_buffer.data(),0,nullptr,&p->read_event),"async snapshot");
 check(p->api.clFlush(p->queue),"snapshot flush");p->timing.snapshot_wall+=elapsed(begin);
}
bool GranularGPU::receive_snapshot(std::vector<Grain>& destination,bool wait){
 if(!p->read_event)return false;auto begin=Clock::now();
 if(wait)check(p->api.clWaitForEvents(1,&p->read_event),"snapshot wait");
 cl_int status=0;check(p->api.clGetEventInfo(p->read_event,CL_EVENT_COMMAND_EXECUTION_STATUS,sizeof(status),&status,nullptr),"snapshot status");require(status>=0,"async snapshot failed");
 if(status!=CL_COMPLETE){p->timing.snapshot_wall+=elapsed(begin);return false;}
 if(p->c.profile){cl_ulong start=0,end=0;check(p->api.clGetEventProfilingInfo(p->read_event,CL_PROFILING_COMMAND_START,sizeof(start),&start,nullptr),"snapshot profile start");check(p->api.clGetEventProfilingInfo(p->read_event,CL_PROFILING_COMMAND_END,sizeof(end),&end,nullptr),"snapshot profile end");p->timing.gpu_seconds[7]+=(end-start)*1.e-9;++p->timing.calls[7];}
 p->api.clReleaseEvent(p->read_event);p->read_event=nullptr;destination.swap(p->read_buffer);p->timing.snapshot_wall+=elapsed(begin);return true;
}
void GranularGPU::set_state(const std::vector<Grain>& a,const std::vector<Grain>& b){p->set(a,b);}
GranularCandidates GranularGPU::inspect_candidates(){
 require(p->c.candidates&&!p->poisoned&&p->c.count<=4096,"candidate inspection requires enabled healthy solver and <=4096 particles");
 int zero=0,n=static_cast<int>(p->c.count);check(p->api.clEnqueueWriteBuffer(p->queue,p->buffers[6],CL_TRUE,0,sizeof(zero),&zero,0,nullptr,nullptr),"diagnostic clear guard");cl_ulong zeroOverflow=0;check(p->api.clEnqueueWriteBuffer(p->queue,p->buffers[12],CL_TRUE,0,sizeof(zeroOverflow),&zeroOverflow,0,nullptr,nullptr),"diagnostic clear overflow");
 if(p->c.candidate_reuse){cl_uint control[6]{1,1,0,0,0,1};check(p->api.clEnqueueWriteBuffer(p->queue,p->buffers[14],CL_TRUE,0,sizeof(control),control,0,nullptr,nullptr),"diagnostic force rebuild");++p->timing.candidate_builds;}
 p->arg(0,0,p->buffers[4]);p->arg(0,1,p->cells);p->run(0,p->cells);
 p->arg(1,0,p->buffers[0]);p->arg(1,1,p->buffers[4]);p->arg(1,2,p->buffers[5]);p->arg(1,3,n);p->arg(1,4,p->cell);p->arg(1,5,p->nx);p->arg(1,6,p->ny);p->arg(1,7,p->nz);p->arg(1,8,p->buffers[6]);p->run(1,n);p->build_candidates();
 auto read=[&](unsigned index,std::vector<int>& data){check(p->api.clEnqueueReadBuffer(p->queue,p->buffers[index],CL_TRUE,0,data.size()*sizeof(int),data.data(),0,nullptr,nullptr),"candidate inspection");};
 std::vector<int> error(1),counts(n),ids(std::size_t(n)*p->c.candidate_capacity),head(p->cells),links(n);read(6,error);require(!error[0],"candidate inspection outside domain");read(11,counts);read(10,ids);read(4,head);read(5,links);auto positions=snapshot();p->collect();GranularCandidates result;result.neighbors.resize(n);
 for(int i=0;i<n;++i){auto& out=result.neighbors[i];if(counts[i]>=0){for(int k=0;k<counts[i];++k)out.push_back(ids[p->c.kernel_optimize?std::size_t(k)*n+i:std::size_t(i)*p->c.candidate_capacity+k]);continue;}
  ++result.overflow_particles;auto a=positions[i];int qx=int(std::floor((a.x+(p->c.test_mixer?.8f:p->c.fast_bounds?.46f:.6f))/p->cell)),qy=int(std::floor((a.y+(p->c.test_mixer?.5f:p->c.fast_bounds?.26f:.4f))/p->cell)),qz=int(std::floor((a.z+(p->c.test_mixer?.5f:p->c.fast_bounds?.61f:.65f))/p->cell));
  for(int z=qz-1;z<=qz+1;++z)for(int y=qy-1;y<=qy+1;++y)for(int x=qx-1;x<=qx+1;++x){if(x<0||y<0||z<0||x>=p->nx||y>=p->ny||z>=p->nz)continue;for(int j=head[(z*p->ny+y)*p->nx+x];j>=0;j=links[j])if(j!=i)out.push_back(j);}
 }
 return result;
}
std::vector<GranularBoundarySample> GranularGPU::inspect_boundaries(const std::vector<Grain>& points,float angle,bool gate,bool shell){
 require(!p->poisoned&&!points.empty()&&points.size()<=65536&&std::isfinite(angle),"invalid boundary diagnostic request");
 for(auto a:points)for(float v:{a.x,a.y,a.z,a.w})require(std::isfinite(v),"nonfinite boundary diagnostic point");
 std::array<cl_mem,4> temp{};try{
 for(auto& entry:p->argument_cache[13])entry.clear(); // temporary diagnostic buffers may reuse handle addresses
 cl_int err=0;if(!p->kernels[13]){p->kernels[13]=p->api.clCreateKernel(p->program,"inspect_boundaries",&err);check(err,"boundary diagnostic kernel");}
 const auto bytes=points.size()*sizeof(Grain);for(auto& b:temp){b=p->api.clCreateBuffer(p->context,CL_MEM_READ_WRITE,bytes,nullptr,&err);check(err,"boundary diagnostic allocation");}
 check(p->api.clEnqueueWriteBuffer(p->queue,temp[0],CL_TRUE,0,bytes,points.data(),0,nullptr,nullptr),"boundary diagnostic upload");
 int n=static_cast<int>(points.size()),g=gate?1:0;float r=p->c.radius*(shell?1.002f:1.f);
 p->arg(13,0,temp[0]);p->arg(13,1,n);p->arg(13,2,r);p->arg(13,3,angle);p->arg(13,4,g);p->arg(13,5,p->buffers[15]);for(unsigned i=1;i<4;++i)p->arg(13,i+5,temp[i]);p->run(13,points.size());
 std::array<std::vector<Grain>,3> data;for(unsigned i=0;i<3;++i){data[i].resize(points.size());check(p->api.clEnqueueReadBuffer(p->queue,temp[i+1],CL_TRUE,0,bytes,data[i].data(),0,nullptr,nullptr),"boundary diagnostic read");}p->collect();
 std::vector<GranularBoundarySample> result(points.size());for(unsigned i=0;i<points.size();++i){result[i]={data[0][i],data[1][i],data[2][i],static_cast<unsigned>(data[1][i].w),static_cast<unsigned>(data[2][i].w)};result[i].normal.w=0;result[i].velocity.w=0;}
 for(auto b:temp)p->api.clReleaseMemObject(b);return result;
 }catch(...){for(auto b:temp)if(b)p->api.clReleaseMemObject(b);throw;}
}
GranularTiming GranularGPU::timing()const{return p->timing;}
double GranularGPU::time()const{return p->time;}
float GranularGPU::radius()const{return p->c.radius;}
float GranularGPU::angle()const{return p->angle;}
std::string GranularGPU::device()const{return p->name;}
void write_granular_timing(std::ostream& out,const GranularTiming& t){
 out<<std::setprecision(12)<<"{\"schema\":5,\"profiling_enabled\":"<<(t.profiling_enabled?"true":"false")<<",\"split_boundaries\":"<<(t.split_boundaries?"true":"false")
 <<",\"advance_wall_s\":"<<t.advance_wall<<",\"snapshot_wall_s\":"<<t.snapshot_wall<<",\"submit_wall_s\":"<<t.submit_wall<<",\"guard_wait_wall_s\":"<<t.guard_wait_wall<<",\"profile_collect_wall_s\":"<<t.profile_collect_wall;
 out<<",\"candidates_enabled\":"<<(t.candidates_enabled?"true":"false")<<",\"candidate_capacity\":"<<t.candidate_capacity<<",\"candidate_builds\":"<<t.candidate_builds<<",\"candidate_fallback_particle_iterations\":"<<t.candidate_fallback_particle_iterations;
 out<<",\"candidate_reuse_enabled\":"<<(t.candidate_reuse_enabled?"true":"false")<<",\"candidate_audit_enabled\":"<<(t.candidate_audit_enabled?"true":"false")<<",\"candidate_skin_m\":"<<t.candidate_skin_m<<",\"reuse_checks\":"<<t.reuse_checks<<",\"candidate_reuses\":"<<t.candidate_reuses;
 out<<",\"kernel_optimize_enabled\":"<<(t.kernel_optimize_enabled?"true":"false")<<",\"argument_calls\":"<<t.argument_calls<<",\"argument_cache_hits\":"<<t.argument_cache_hits;
 out<<",\"boundary_candidates_enabled\":"<<(t.boundary_candidates_enabled?"true":"false");
 const char* names[]={"grid_clear","grid_build","predict",t.split_boundaries?"particle_contact":"contact_and_boundary",t.split_boundaries?"velocity":"velocity_and_boundary_probe","boundary_projection","boundary_probe","snapshot_readback","candidate_build","reuse_begin","reuse_check","candidate_audit","boundary_group_init","boundary_inspect"};
 for(unsigned i=0;i<14;++i)out<<",\""<<names[i]<<"_gpu_s\":"<<t.gpu_seconds[i]<<",\""<<names[i]<<"_calls\":"<<t.calls[i];
 out<<"}";
}

}
