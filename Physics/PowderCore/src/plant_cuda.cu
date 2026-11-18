#include "powder/plant_cuda.hpp"
#include "powder/plant.hpp"
#include "powder/plant_candidates.hpp"
#include "cuda_vectors.cuh"
#include <chrono>
#include <cstring>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <cuda.h>
namespace powder { namespace cuda_detail {
#include "cuda_plant_kernels.cuh"
} }
namespace powder {
namespace {
using Timer=std::chrono::steady_clock;
double seconds(Timer::time_point t){return std::chrono::duration<double>(Timer::now()-t).count();}
void cucheck(cudaError_t e,const char* s){if(e!=cudaSuccess)throw std::runtime_error(std::string(s)+": "+cudaGetErrorString(e));}
void checkbool(bool b,const char*s){if(!b)throw std::invalid_argument(s);}
void drivercheck(CUresult e,const char*s){if(e!=CUDA_SUCCESS){const char*msg=nullptr;cuGetErrorString(e,&msg);throw std::runtime_error(std::string(s)+": "+(msg?msg:"CUDA driver error"));}}
}
struct PlantCuda::Impl {
 GranularConfig c;cudaStream_t stream=nullptr;std::array<void*,16>b{};
 CUmodule module=nullptr;std::array<CUfunction,8>functions{};
 std::vector<Grain> upload;std::vector<std::pair<size_t,Grain>> patches;
 size_t cache_offset=0,cache_count=0,extra=0;Grain threaded_header{},cache_header{};
 GranularTiming profile;double sim=0;float phase=0;bool poisoned=false;std::string name;
 cudaGraph_t graph=nullptr;cudaGraphExec_t executable=nullptr;unsigned graph_steps=0;
 float *device_angles=nullptr,*host_angles=nullptr;
 struct Event {cudaEvent_t first,last;unsigned category;};std::vector<Event> events;
 Impl(GranularConfig cfg,const std::vector<Grain>&p,const std::vector<Grain>&v):c(std::move(cfg)){
  try{
   checkbool(c.plant&&c.plant_threaded&&c.plant_wall_mode==5&&c.fast_bounds,"CUDA plant requires threaded mode 5 and default grid offset");
   checkbool(!c.binary_materials&&!c.rotation&&!c.candidates&&!c.coupled_air&&!c.air.enabled&&!c.boundary_candidates&&!c.cohesion_gamma_j_m2&&!c.wall_adhesion_gamma_j_m2&&!c.profile_split&&!c.plant_boundary_audit&&!c.plant_triangle_fast,"unsupported CUDA plant model; no implicit conversion");
   checkbool(c.count>0&&c.count<=1000000&&c.iterations>=2&&c.iterations<=32&&c.diagnostic_local_size==128&&c.dt>0&&c.dt<=.001f&&std::isfinite(c.dt)&&c.friction>=0&&c.friction<=1,"invalid CUDA controls");
   validate_plant_data(c.plant_data);validate_granular_initial(c,p);
   int devices=0;cucheck(cudaGetDeviceCount(&devices),"CUDA device count");checkbool(devices>0,"no CUDA device");cucheck(cudaSetDevice(0),"CUDA device");cudaDeviceProp prop{};cucheck(cudaGetDeviceProperties(&prop,0),"CUDA properties");name=std::string(prop.name)+" / native CUDA";
   cucheck(cudaStreamCreateWithFlags(&stream,cudaStreamNonBlocking),"CUDA stream");
   if(c.backend==GranularBackend::CudaPtx||c.backend==GranularBackend::CudaPtxGraph){
    drivercheck(cuModuleLoad(&module,c.cuda_ptx_file.c_str()),"CUDA PTX module load");
    const char*names[]={"predict","cleargrid","grid","solve","velocity","inspect_boundaries","solve_graph","velocity_graph"};
    for(unsigned i=0;i<functions.size();++i)drivercheck(cuModuleGetFunction(&functions[i],module,names[i]),"CUDA PTX kernel");
    name=std::string(prop.name)+" / CUDA Driver frozen PTX";
   }
   for(unsigned i=0;i<8;++i){size_t bytes=(i<4||i==7)?size_t(c.count)*sizeof(Grain):i==4?262144*sizeof(int):i==5?size_t(c.count)*sizeof(int):sizeof(int);cucheck(cudaMalloc(&b[i],bytes),"CUDA state allocation");}
   upload=plant_threaded_upload(c.plant_data);threaded_header=upload[8];auto size=upload.size();plant_candidate_upload(upload,c.plant_wall_mode);extra=(upload.size()-size)*sizeof(Grain);
   for(size_t i=0;i<c.plant_data.size();++i)if(std::memcmp(&upload[i],&c.plant_data[i],sizeof(Grain)))patches.push_back({i,upload[i]});
   checkbool(upload[10].x==2&&upload[13].z>0,"CUDA requires indexed hybrid plant");cache_offset=upload.size();cache_count=std::min(size_t(c.count),size_t(65536));upload[10].z=float(cache_offset);upload[10].w=float(cache_count);upload.resize(upload.size()+cache_count*139);cache_header=upload[10];
   checkbool(upload.size()<16777216,"CUDA plant indices exceed FP32 exact range");cucheck(cudaMalloc(&b[15],upload.size()*sizeof(Grain)),"CUDA plant allocation");cucheck(cudaMemcpyAsync(b[15],upload.data(),upload.size()*sizeof(Grain),cudaMemcpyHostToDevice,stream),"CUDA plant upload");
   cucheck(cudaMalloc(&device_angles,1000*sizeof(float)),"CUDA angles");cucheck(cudaMallocHost(&host_angles,1000*sizeof(float)),"CUDA pinned angles");set(p,v);
  }catch(...){release();throw;}
 }
 ~Impl(){release();}
 void release(){if(stream)cudaStreamSynchronize(stream);for(auto&e:events){cudaEventDestroy(e.first);cudaEventDestroy(e.last);}events.clear();if(executable)cudaGraphExecDestroy(executable);if(graph)cudaGraphDestroy(graph);for(auto&x:b)if(x){cudaFree(x);x=nullptr;}if(device_angles)cudaFree(device_angles);if(host_angles)cudaFreeHost(host_angles);if(module)cuModuleUnload(module);if(stream)cudaStreamDestroy(stream);stream=nullptr;}
 void clear_cache(){cucheck(cudaMemsetAsync(static_cast<Grain*>(b[15])+cache_offset,0,cache_count*139*sizeof(Grain),stream),"CUDA cache clear");}
 void set(const std::vector<Grain>&p,const std::vector<Grain>&v){checkbool(p.size()==c.count&&v.size()==c.count,"CUDA state count mismatch");for(size_t i=0;i<p.size();++i){for(float x:{p[i].x,p[i].y,p[i].z,p[i].w,v[i].x,v[i].y,v[i].z})checkbool(std::isfinite(x),"CUDA nonfinite state");checkbool(p[i].w>=0&&p[i].w<32&&std::floor(p[i].w)==p[i].w,"CUDA invalid group");}
  clear_cache();cucheck(cudaMemcpyAsync(b[0],p.data(),p.size()*sizeof(Grain),cudaMemcpyHostToDevice,stream),"CUDA positions reset");cucheck(cudaMemcpyAsync(b[2],v.data(),v.size()*sizeof(Grain),cudaMemcpyHostToDevice,stream),"CUDA velocity reset");cucheck(cudaStreamSynchronize(stream),"CUDA reset sync");sim=0;phase=0;poisoned=false;profile={};profile.profiling_enabled=c.profile;
 }
 template<class F> void launch(unsigned cat,F f){if(c.profile){Event e{};e.category=cat;cucheck(cudaEventCreate(&e.first),"CUDA event");cucheck(cudaEventCreate(&e.last),"CUDA event");cucheck(cudaEventRecord(e.first,stream),"CUDA event start");f();cucheck(cudaEventRecord(e.last,stream),"CUDA event stop");events.push_back(e);}else f();cucheck(cudaGetLastError(),"CUDA kernel launch");++profile.calls[cat];}
 template<class... Args> void ptx(unsigned kernel,unsigned blocks,Args... args){void*parameters[]={static_cast<void*>(&args)...};drivercheck(cuLaunchKernel(functions[kernel],blocks,1,1,128,1,1,0,stream,parameters,nullptr),"CUDA PTX launch");}
 void dispatch(unsigned steps,float omega,bool useGraph){
  using namespace cuda_detail;const unsigned blocks=(c.count+127)/128;int n=int(c.count);float cell=2*c.radius;
  auto f=[&](int i){return reinterpret_cast<cuda_detail::float4*>(b[i]);};
  auto h=static_cast<int*>(b[4]),link=static_cast<int*>(b[5]),err=static_cast<int*>(b[6]);
  for(unsigned s=0;s<steps;++s){
   float a=host_angles[s];
   launch(2,[&]{if(module)ptx(0,blocks,b[0],b[1],b[2],n,c.dt,c.radius,err,b[7],b[15]);else cuda_detail::predict<<<blocks,128,0,stream>>>(f(0),f(1),f(2),n,c.dt,c.radius,err,f(7),f(15));});
   for(unsigned it=0;it<c.iterations;++it){
    // All bits set is the same -1 sentinel as cleargrid. Graph dependencies still
    // complete this full reset before grid insertion; no cells or contacts skipped.
    launch(0,[&]{if(useGraph)cucheck(cudaMemsetAsync(h,0xff,262144*sizeof(int),stream),"CUDA graph grid reset");else if(module)ptx(1,2048,h,262144);else cuda_detail::cleargrid<<<2048,128,0,stream>>>(h,262144);});
    launch(1,[&]{if(module)ptx(2,blocks,b[0],h,link,n,cell,1,1,1,err);else cuda_detail::grid<<<blocks,128,0,stream>>>(f(0),h,link,n,cell,1,1,1,err);});
    launch(3,[&]{if(module){if(useGraph)ptx(6,blocks,b[0],b[1],b[3],h,link,n,c.radius,cell,1,1,1,device_angles,s,0,c.friction,b[15],err);else ptx(3,blocks,b[0],b[1],b[3],h,link,n,c.radius,cell,1,1,1,a,0,c.friction,b[15],err);}else if(useGraph)cuda_detail::solve_graph<<<blocks,128,0,stream>>>(f(0),f(1),f(3),h,link,n,c.radius,cell,1,1,1,device_angles,s,0,c.friction,f(15),err);else cuda_detail::solve<<<blocks,128,0,stream>>>(f(0),f(1),f(3),h,link,n,c.radius,cell,1,1,1,a,0,c.friction,f(15),err);});
    std::swap(b[0],b[3]);
   }
   launch(4,[&]{if(module){if(useGraph)ptx(7,blocks,b[0],b[1],b[2],n,c.dt,c.radius,device_angles,s,omega,0,c.friction,err,b[7],b[15]);else ptx(4,blocks,b[0],b[1],b[2],n,c.dt,c.radius,a,omega,0,c.friction,err,b[7],b[15]);}else if(useGraph)cuda_detail::velocity_graph<<<blocks,128,0,stream>>>(f(0),f(1),f(2),n,c.dt,c.radius,device_angles,s,omega,0,c.friction,err,f(7),f(15));else cuda_detail::velocity<<<blocks,128,0,stream>>>(f(0),f(1),f(2),n,c.dt,c.radius,a,omega,0,c.friction,err,f(7),f(15));});
  }
 }
 float graph_omega=0;
 void advance(unsigned steps,float rpm,bool gate){checkbool(!poisoned,"CUDA reset required after failure");checkbool(steps>0&&steps<=1000&&std::isfinite(rpm)&&std::abs(rpm)<=60&&!gate,"invalid CUDA plant advance");auto start=Timer::now();float omega=rpm*.104719755f,a=phase;
  for(unsigned s=0;s<steps;++s){a+=omega*c.dt;host_angles[s]=a;}
  cucheck(cudaMemsetAsync(b[6],0,sizeof(int),stream),"CUDA guard reset");
  bool optimized=c.backend==GranularBackend::CudaGraph||c.backend==GranularBackend::CudaPtxGraph;
  if(optimized){checkbool(!c.profile&&c.iterations%2==0,"CUDA graph requires even iterations and profiling off");cucheck(cudaMemcpyAsync(device_angles,host_angles,steps*sizeof(float),cudaMemcpyHostToDevice,stream),"CUDA angle update");
   if(!executable||graph_steps!=steps||graph_omega!=omega){
    cucheck(cudaStreamSynchronize(stream),"CUDA graph setup sync");if(executable){cucheck(cudaGraphExecDestroy(executable),"CUDA graph release");executable=nullptr;}if(graph){cucheck(cudaGraphDestroy(graph),"CUDA graph release");graph=nullptr;}
    cucheck(cudaStreamBeginCapture(stream,cudaStreamCaptureModeThreadLocal),"CUDA capture begin");dispatch(steps,omega,true);cucheck(cudaStreamEndCapture(stream,&graph),"CUDA capture end");cucheck(cudaGraphInstantiate(&executable,graph,nullptr,nullptr,0),"CUDA graph instantiate");graph_steps=steps;graph_omega=omega;
   }else {profile.calls[0]+=steps*c.iterations;profile.calls[1]+=steps*c.iterations;profile.calls[2]+=steps;profile.calls[3]+=steps*c.iterations;profile.calls[4]+=steps;}
   cucheck(cudaGraphLaunch(executable,stream),"CUDA graph launch");
  }else dispatch(steps,omega,false);
  profile.submit_wall+=seconds(start);auto wait=Timer::now();int error=0;cucheck(cudaMemcpyAsync(&error,b[6],sizeof(error),cudaMemcpyDeviceToHost,stream),"CUDA guard download");cucheck(cudaStreamSynchronize(stream),"CUDA batch completion");profile.guard_wait_wall+=seconds(wait);
  for(auto&e:events){float ms=0;cucheck(cudaEventElapsedTime(&ms,e.first,e.last),"CUDA event time");profile.gpu_seconds[e.category]+=ms*.001;cudaEventDestroy(e.first);cudaEventDestroy(e.last);}events.clear();profile.advance_wall+=seconds(start);
  if(error){poisoned=true;throw std::runtime_error("CUDA plant safety bits="+std::to_string(error)+"; reset required");}phase=a;sim+=steps*c.dt;
 }
 std::vector<Grain> snapshot(bool velocity){auto start=Timer::now();std::vector<Grain> r(c.count);cucheck(cudaMemcpyAsync(r.data(),b[velocity?2:0],r.size()*sizeof(Grain),cudaMemcpyDeviceToHost,stream),"CUDA snapshot");cucheck(cudaStreamSynchronize(stream),"CUDA snapshot sync");profile.snapshot_wall+=seconds(start);return r;}
 void update(const std::vector<Grain>&data){checkbool(!poisoned&&data.size()==c.plant_data.size(),"CUDA field mismatch");validate_plant_data(data);for(size_t i=0;i<data.size();++i){auto a=data[i],ref=c.plant_data[i];if(i==4)a=ref;else if(i>=size_t(data[5].x)&&i<size_t(data[5].x+2*data[5].y)&&(i-size_t(data[5].x))%2==1&&ref.x<=2){a.y=ref.y;a.z=ref.z;}else if(i>=size_t(data[0].z)&&i<size_t(data[0].z+3*data[0].w)&&(i-size_t(data[0].z))%3==2)a.x=ref.x;checkbool(!std::memcmp(&a,&ref,sizeof(Grain)),"CUDA plant geometry immutable");}
  auto next=data;for(auto patch:patches)next[patch.first]=patch.second;next[8]=threaded_header;next[10]=cache_header;clear_cache();cucheck(cudaMemcpyAsync(b[15],next.data(),next.size()*sizeof(Grain),cudaMemcpyHostToDevice,stream),"CUDA field upload");cucheck(cudaStreamSynchronize(stream),"CUDA field sync");
 }
 std::vector<GranularBoundarySample> inspect(const std::vector<Grain>&points,float a,bool gate,bool shell){checkbool(!poisoned&&!points.empty()&&points.size()<=65536&&std::isfinite(a),"CUDA boundary inspection input");for(auto x:points)for(float v:{x.x,x.y,x.z,x.w})checkbool(std::isfinite(v),"CUDA nonfinite probe");std::array<void*,4>temp{};try{clear_cache();for(auto&x:temp)cucheck(cudaMalloc(&x,points.size()*sizeof(Grain)),"CUDA probe allocate");cucheck(cudaMemcpyAsync(temp[0],points.data(),points.size()*sizeof(Grain),cudaMemcpyHostToDevice,stream),"CUDA probe upload");
  auto f=[&](int i){return reinterpret_cast<cuda_detail::float4*>(temp[i]);};if(module)ptx(5,unsigned((points.size()+127)/128),temp[0],int(points.size()),c.radius*(shell?1.002f:1.f),a,int(gate),b[15],temp[1],temp[2],temp[3]);else cuda_detail::inspect_boundaries<<<(points.size()+127)/128,128,0,stream>>>(f(0),int(points.size()),c.radius*(shell?1.002f:1.f),a,int(gate),reinterpret_cast<cuda_detail::float4*>(b[15]),f(1),f(2),f(3));cucheck(cudaGetLastError(),"CUDA probe launch");
  std::array<std::vector<Grain>,3>out;for(unsigned i=0;i<3;++i){out[i].resize(points.size());cucheck(cudaMemcpyAsync(out[i].data(),temp[i+1],points.size()*sizeof(Grain),cudaMemcpyDeviceToHost,stream),"CUDA probe read");}cucheck(cudaStreamSynchronize(stream),"CUDA probe sync");std::vector<GranularBoundarySample>r(points.size());for(size_t i=0;i<points.size();++i)r[i]={out[0][i],out[1][i],out[2][i],0,0};for(auto x:temp)cudaFree(x);clear_cache();return r;
  }catch(...){cudaStreamSynchronize(stream);for(auto x:temp)if(x)cudaFree(x);throw;}
 }
};
PlantCuda::PlantCuda(GranularConfig c,const std::vector<Grain>&a,const std::vector<Grain>&v):p(std::make_unique<Impl>(c,a,v)){}
PlantCuda::~PlantCuda()=default;
void PlantCuda::advance(unsigned n,float rpm,bool gate){try{p->advance(n,rpm,gate);}catch(...){p->poisoned=true;throw;}}
void PlantCuda::set(const std::vector<Grain>&a,const std::vector<Grain>&v){p->set(a,v);}
void PlantCuda::update(const std::vector<Grain>&d){p->update(d);}
std::vector<Grain> PlantCuda::snapshot(bool v){return p->snapshot(v);}
std::vector<GranularBoundarySample> PlantCuda::inspect(const std::vector<Grain>&a,float angle,bool g,bool s){return p->inspect(a,angle,g,s);}
GranularTiming PlantCuda::timing()const{return p->profile;}
double PlantCuda::time()const{return p->sim;}float PlantCuda::angle()const{return p->phase;}std::string PlantCuda::device()const{return p->name;}
std::array<std::uint64_t,3> PlantCuda::memory()const{return {p->upload.size()*sizeof(Grain),p->extra,p->cache_count*139*sizeof(Grain)};}
std::array<std::uint64_t,4> PlantCuda::stats(){std::vector<Grain> cache(p->cache_count*139);cucheck(cudaMemcpyAsync(cache.data(),static_cast<Grain*>(p->b[15])+p->cache_offset,cache.size()*sizeof(Grain),cudaMemcpyDeviceToHost,p->stream),"CUDA cache read");cucheck(cudaStreamSynchronize(p->stream),"CUDA cache sync");std::array<std::uint64_t,4>r{};for(size_t i=0;i<p->cache_count;++i){std::uint32_t values[4];std::memcpy(values,&cache[i*139+138],16);for(unsigned j=0;j<4;++j)r[j]+=values[j];}return r;}
}
