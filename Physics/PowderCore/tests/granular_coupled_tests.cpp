#include "powder/granular.hpp"
#include "powder/coupled.hpp"
#include "powder/granular_coupled.hpp"
#include <iostream>
#include <cmath>
#include <iomanip>
using namespace powder;
void ok(bool b,const char* s){if(!b)throw std::runtime_error(s);}
template<class F>void rejects(F f){bool threw=false;try{f();}catch(const std::exception&){threw=true;}ok(threw,"expected guard rejection");}
int main(){try{
 GranularConfig c;c.coupled_air=true;c.binary_materials=true;c.count=2;c.dt=.000375f;c.materials[0].radius_m=.006;c.materials[0].density_kg_m3=900;c.materials[1].radius_m=.005;c.materials[1].density_kg_m3=1500;
 std::vector<Grain> initial{{.3f,.18f,.35f,0},{-.3f,.18f,.35f,1}},vel{{.1f,0,0,0},{-.1f,0,0,0}};
 GranularGasField f;f.cells={4,4,4};f.origin={-.5f,-.5f,-.5f};f.h=.25f;f.velocity_density.assign(64,{2,.5f,0,1.2f});f.gradient_epsilon.assign(64,{100,0,20,.7f});
 for(bool optimized:{false,true}){c.candidates=optimized;c.candidate_reuse=optimized;c.kernel_optimize=optimized;c.rotation=optimized;GranularGPU gpu(c,initial,vel);gpu.set_gas_field(f);gpu.advance(200,0,false);auto v=gpu.velocity_snapshot();auto ex=gpu.gas_exchange();double impulse[3]{},vol=0;for(auto a:ex.impulse_volume){impulse[0]+=a.x;impulse[1]+=a.y;impulse[2]+=a.z;vol+=a.w;}
 double predictedImpulse[3]{},physicalVolume=0,maxerr=0;
 for(int g=0;g<2;++g){double q[3]{vel[g].x,vel[g].y,vel[g].z},air[3]{2,.5,0},grad[3]{100,0,20},rho=c.materials[g].density_kg_m3,r=c.materials[g].radius_m,m=c.materials[g].mass_kg();physicalVolume+=m/rho;
  for(int step=0;step<200;++step){double slip=std::hypot(air[0]-q[0],air[1]-q[1],air[2]-q[2]);double beta=dense_drag_beta(2*r,rho,1.2f,f.viscosity,.7f,slip);for(int k=0;k<3;++k)q[k]=(q[k]+c.dt*((k==2?-9.81:0)+beta*air[k]-grad[k]/rho))/(1+c.dt*beta);}
  maxerr=std::max(maxerr,std::hypot(v[g].x-q[0],v[g].y-q[1],v[g].z-q[2]));predictedImpulse[0]+=m*(v[g].x-vel[g].x);predictedImpulse[1]+=m*(v[g].y-vel[g].y);predictedImpulse[2]+=m*(v[g].z-vel[g].z+9.81*200*c.dt);
 }
 double balance=std::hypot(impulse[0]+predictedImpulse[0],impulse[1]+predictedImpulse[1],impulse[2]+predictedImpulse[2]);std::cout<<std::setprecision(12)<<"optimized="<<optimized<<" velocity_error="<<maxerr<<" exchange_Ns="<<balance<<" volume_error="<<vol-physicalVolume<<'\n';ok(maxerr<2e-5,"dense grid CPU/GPU velocity parity");ok(balance<2e-9,"equal/opposite gas impulse");ok(std::abs(vol-physicalVolume)<physicalVolume*1e-5,"CIC volume conservation");ok(ex.steps==200,"exchange step ledger");gpu.set_gas_field(f);auto reset=gpu.gas_exchange();ok(reset.steps==0,"field reset exchange clock");for(auto a:reset.impulse_volume)ok(a.x==0&&a.y==0&&a.z==0&&a.w==0,"field reset zero ledger");
 }
 {
 auto cfg=c;cfg.dt=1e-5f;cfg.candidates=cfg.candidate_reuse=cfg.kernel_optimize=cfg.rotation=false;
 GranularGPU gpu(cfg,initial,vel);GasConfig gasConfig;gasConfig.cells={4,4,4};gasConfig.h=.25;gasConfig.boundary_pa={};GranularGasCoupler bridge(cfg,initial,GasGrid(gasConfig),{-.5,-.5,-.5});
 CoupledWorld reference(gasConfig);reference.dem=coupled_demo(1,0).dem;reference.dem.planes.clear();reference.dem.bodies.clear();
 for(int i=0;i<2;++i){auto b=make_clump(i+1,i,{{{},cfg.materials[i].radius_m*2}},cfg.materials[i].density_kg_m3);b.position_m={initial[i].x+.5,initial[i].y+.5,initial[i].z+.5};b.velocity_m_s={vel[i].x,vel[i].y,vel[i].z};reference.dem.bodies.push_back(b);}reference.initialize();
 for(int i=0;i<200;++i){bridge.advance(gpu,1,0,false);reference.step(cfg.dt);}
 auto actual=gpu.velocity_snapshot();double difference=0;for(int i=0;i<2;++i){auto v=reference.dem.bodies[i].velocity_m_s;difference=std::max(difference,std::hypot(actual[i].x-v.x,actual[i].y-v.y,actual[i].z-v.z));}
 std::cout<<"coupled reference velocity error="<<difference<<" gas balance="<<bridge.gas_mass_error()<<'\n';ok(difference<2e-5,"GPU/CPU two-way evolution parity");ok(std::abs(bridge.gas_mass_error())<1e-12,"two-way gas mass conservation");ok(std::abs(bridge.elapsed_s()-reference.report().time)<1e-12,"coupled clock parity");
 }
 {
 auto box=c;box.simple_box=true;box.test_mixer=true;box.candidates=box.candidate_reuse=box.kernel_optimize=box.rotation=false;box.count=1;
 GranularGPU gpu(box,{{0,0,-.48f,0}},{{0,0,0,0}});GranularGasField boxField;boxField.cells={16,10,10};boxField.h=.1f;boxField.origin={-.8f,-.5f,-.5f};boxField.velocity_density.assign(1600,{0,0,0,1.2f});boxField.gradient_epsilon.assign(1600,{0,0,0,1});gpu.set_gas_field(boxField);gpu.advance(400,0,false);auto p=gpu.snapshot()[0];ok(p.z>=-.5+box.materials[0].radius_m-1e-6,"simple gas box floor prevents penetration");auto hits=gpu.inspect_boundaries({{.799f,0,0,0},{0,.499f,0,0},{0,0,-.499f,0}},0);ok(hits[0].normal.x<-.99&&hits[1].normal.y<-.99&&hits[2].normal.z>.99,"simple box face normals");
 }
 {
 // Virtual contact attraction: critical wall load is 4*pi*gamma*r versus mg.
 auto sticky=c;sticky.coupled_air=false;sticky.simple_box=true;sticky.test_mixer=true;sticky.count=1;sticky.candidates=sticky.candidate_reuse=sticky.kernel_optimize=true;sticky.rotation=false;sticky.wall_adhesion_gamma_j_m2=.3f;
 GranularGPU held(sticky,{{0,0,float(.5-sticky.materials[0].radius_m),0}},{{0,0,0,0}});held.advance(400,0,false);ok(held.snapshot()[0].z>.5-sticky.materials[0].radius_m-1e-6,"GPU contact attraction holds ceiling against gravity");
 {
 auto wind=sticky;wind.coupled_air=true;GranularGPU blown(wind,{{0,0,float(.5-wind.materials[0].radius_m),0}},{{0,0,0,0}});GranularGasField flow;flow.cells={16,10,10};flow.origin={-.8f,-.5f,-.5f};flow.h=.1f;flow.velocity_density.assign(1600,{0,0,-50,1.2f});flow.gradient_epsilon.assign(1600,{0,0,0,1});blown.set_gas_field(flow);blown.advance(1,0,false);auto exchange=blown.gas_exchange();double impulse=0;for(auto a:exchange.impulse_volume)impulse+=a.z;
 double radius=wind.materials[0].radius_m,m=wind.materials[0].mass_kg(),beta=dense_drag_beta(radius*2,wind.materials[0].density_kg_m3,1.2f,flow.viscosity,1,50),external=-9.81+4*3.141592653589793*wind.wall_adhesion_gamma_j_m2*radius/m;double predicted=wind.dt*(external-50*beta)/(1+wind.dt*beta),expected=-m*(predicted-wind.dt*external);ok(std::abs(impulse-expected)<2e-9,"gas reaction excludes adhesive/wall source load");blown.advance(199,0,false);ok(blown.snapshot()[0].z<.45,"air load detaches GPU wall contact");
 }
 sticky.wall_adhesion_gamma_j_m2=.01f;GranularGPU released(sticky,{{0,0,float(.5-sticky.materials[0].radius_m),0}},{{0,0,0,0}});released.advance(400,0,false);ok(released.snapshot()[0].z<.45,"GPU contact attraction releases below gravity load");
 sticky.count=2;sticky.wall_adhesion_gamma_j_m2=0;sticky.cohesion_gamma_j_m2=.05f;sticky.dt=1e-5f;sticky.candidates=sticky.candidate_reuse=sticky.kernel_optimize=false;
 double ra=sticky.materials[0].radius_m,rb=sticky.materials[1].radius_m;std::vector<Grain> pair{{0,0,.3f,0},{float(ra+rb),0,.3f,1}};GranularGPU bonded(sticky,pair,{{0,0,0,0},{0,0,0,0}});bonded.advance(100,0,false);auto v=bonded.velocity_snapshot();double momentum=sticky.materials[0].mass_kg()*v[0].x+sticky.materials[1].mass_kg()*v[1].x;std::cout<<"cohesive momentum="<<momentum<<" vx="<<v[0].x<<","<<v[1].x<<"\n";ok(std::abs(momentum)<2e-8,"cohesive pair action reaction");
 sticky.candidates=sticky.candidate_reuse=sticky.kernel_optimize=true;
 GranularGPU cachedBond(sticky,pair,{{0,0,0,0},{0,0,0,0}});cachedBond.advance(100,0,false);auto cv=cachedBond.velocity_snapshot();auto cp=cachedBond.snapshot();auto bp=bonded.snapshot();
 for(int i=0;i<2;++i){ok(std::hypot(cv[i].x-v[i].x,cv[i].y-v[i].y,cv[i].z-v[i].z)<1e-7,"cached cohesive velocity parity");ok(std::hypot(cp[i].x-bp[i].x,cp[i].y-bp[i].y,cp[i].z-bp[i].z)<1e-7,"cached cohesive position parity");}

 }
 c.candidates=c.candidate_reuse=c.kernel_optimize=c.rotation=false;GranularGPU gpu(c,initial,vel);auto bad=f;bad.gradient_epsilon[0].w=.1f;rejects([&]{gpu.set_gas_field(bad);});bad=f;bad.gradient_epsilon.pop_back();rejects([&]{gpu.set_gas_field(bad);});bad=f;bad.h=.001f;rejects([&]{gpu.set_gas_field(bad);});gpu.set_gas_field(f);gpu.advance(1,0,false);
 GranularGPU missing(c,initial,vel);rejects([&]{missing.advance(1,0,false);});
 GranularGPU solid(c,initial,vel);for(auto& a:f.gradient_epsilon)a.w=0;solid.set_gas_field(f);rejects([&]{solid.advance(1,0,false);});rejects([&]{solid.gas_exchange();});
 std::cout<<"PASS GPU gas gather/scatter, dense drag, pressure, volume, impulse, reset, input and solid guards\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
