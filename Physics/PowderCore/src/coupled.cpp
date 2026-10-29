#include "powder/coupled.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <stdexcept>
namespace powder { namespace {
constexpr double R=287.05,pi=3.14159265358979323846;
void check(bool b,const char* s){if(!b)throw std::invalid_argument(s);}
Vec3 plus(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 times(Vec3 a,double s){return {a.x*s,a.y*s,a.z*s};}
double len(Vec3 a){return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);}
double axis(Vec3 a,int k){return k==0?a.x:k==1?a.y:a.z;}
void component(Vec3& a,int k,double v){if(k==0)a.x=v;else if(k==1)a.y=v;else a.z=v;}
Vec3 cross3(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double volume(const Particle& p){return pi/6*p.diameter_m*p.diameter_m*p.diameter_m;}
double body_mass(const ClumpBody& b){double mass=0;for(const auto& p:b.pebbles)mass+=sphere_properties(p.diameter_m,{b.density_kg_m3}).mass_kg;return mass;}
}
void GasConfig::validate()const{
 for(int n:cells)check(n>=2&&n<=64,"gas dimensions must be 2..64");
 check(std::isfinite(h)&&h>=.001&&h<=1,"gas cell size outside 0.001..1m");
 check(std::isfinite(temperature_k)&&temperature_k>=200&&temperature_k<=500,"gas temperature must be 200..500 K");
 check(std::isfinite(initial_pressure_pa)&&initial_pressure_pa>=5000&&initial_pressure_pa<=200000,"initial gas pressure outside 5..200kPa absolute");
 for(double p:boundary_pa)check(std::isfinite(p)&&(p==0||(p>=5000&&p<=200000)),"pressure reservoir outside 5..200kPa absolute");
}
double GasConfig::viscosity()const{return 1.716e-5*std::pow(temperature_k/273.15,1.5)*(273.15+110.4)/(temperature_k+110.4);}
GasGrid::GasGrid(GasConfig c):config(c){c.validate();cells.resize(static_cast<std::size_t>(c.cells[0]*c.cells[1]*c.cells[2]));initialize_pressure();}
int GasGrid::index(int x,int y,int z)const{return (z*config.cells[1]+y)*config.cells[0]+x;}
Vec3 GasGrid::centre(int i)const{return {(i%config.cells[0]+.5)*config.h,((i/config.cells[0])%config.cells[1]+.5)*config.h,(i/(config.cells[0]*config.cells[1])+.5)*config.h};}
Vec3 GasGrid::extent()const{return {config.cells[0]*config.h,config.cells[1]*config.h,config.cells[2]*config.h};}
double GasGrid::pressure(int i)const{const auto& c=cells.at(i);return c.mass/(c.epsilon*std::pow(config.h,3))*R*config.temperature_k;}
Vec3 GasGrid::velocity(int i)const{const auto& c=cells.at(i);return times(c.momentum,1/c.mass);}
double GasGrid::mass()const{double m=0;for(auto& c:cells)if(!c.solid)m+=c.mass;return m;}
void GasGrid::initialize_pressure(){for(auto& c:cells){c.mass=c.solid?0:config.initial_pressure_pa/(R*config.temperature_k)*c.epsilon*std::pow(config.h,3);c.momentum={};}boundary_mass_out=0;}
std::vector<std::pair<int,double>> GasGrid::stencil(Vec3 p)const{
 const auto e=extent();check(p.x>=0&&p.y>=0&&p.z>=0&&p.x<=e.x&&p.y<=e.y&&p.z<=e.z,"particle outside gas grid");
 int base[3];double f[3];for(int k=0;k<3;++k){double q=axis(p,k)/config.h-.5;base[k]=int(std::floor(q));f[k]=q-base[k];}
 check(!cells[index(std::min(int(p.x/config.h),config.cells[0]-1),std::min(int(p.y/config.h),config.cells[1]-1),std::min(int(p.z/config.h),config.cells[2]-1))].solid,"particle centre inside solid gas cell");
 std::vector<std::pair<int,double>> weights;weights.reserve(8);double sum=0;
 for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x){int i=index(std::clamp(base[0]+x,0,config.cells[0]-1),std::clamp(base[1]+y,0,config.cells[1]-1),std::clamp(base[2]+z,0,config.cells[2]-1));double w=(x?f[0]:1-f[0])*(y?f[1]:1-f[1])*(z?f[2]:1-f[2]);if(!cells[i].solid&&w>0){auto found=std::find_if(weights.begin(),weights.end(),[&](const auto& a){return a.first==i;});if(found==weights.end())weights.emplace_back(i,w);else found->second+=w;sum+=w;}}
 check(sum>0,"particle volume inside solid gas mask");for(auto& w:weights)w.second/=sum;return weights;
}
void GasGrid::map_volume(const std::vector<ClumpBody>& bodies){
 std::vector<double> alpha(cells.size());const double v=std::pow(config.h,3);
 for(const auto& b:bodies)for(const auto& p:clump_particles(b)){
  check(p.diameter_m<=config.h*.5,"unresolved coupling requires pebble diameter <= half gas cell size");
  auto e=extent();if(p.position_m.x<0||p.position_m.x>e.x||p.position_m.y<0||p.position_m.y>e.y||p.position_m.z<0||p.position_m.z>e.z)continue;
  for(auto w:stencil(p.position_m))alpha[w.first]+=volume(p)*w.second/v;
 }
 for(std::size_t i=0;i<cells.size();++i){check(alpha[i]<=.65,"gas solid fraction exceeds 0.65: refine particle/grid representation");cells[i].epsilon=1-alpha[i];}
}
double GasGrid::explicit_dt_limit(double cfl)const{
 config.validate();check(std::isfinite(cfl)&&cfl>0&&cfl<=.4,"invalid gas CFL request");double maxwave=0,minrho=1e100;const double v=config.h*config.h*config.h,c=std::sqrt(R*config.temperature_k);
 for(std::size_t i=0;i<cells.size();++i)if(!cells[i].solid){const auto& q=cells[i];check(std::isfinite(q.mass)&&q.mass>0&&q.epsilon>=.35&&q.epsilon<=1,"invalid gas state for dt");double u=len(velocity(int(i)));check(std::isfinite(u)&&u<=100,"gas speed outside dt contract");maxwave=std::max(maxwave,u+c/std::sqrt(q.epsilon));minrho=std::min(minrho,q.mass/v);}
 check(maxwave>0,"gas grid has no fluid cells");return std::min(cfl*config.h/(3*maxwave),cfl*minrho*config.h*config.h/(6*config.viscosity()));
}
GasReport GasGrid::step(double dt,Vec3 gravity){
 config.validate();check(cells.size()==static_cast<std::size_t>(config.cells[0]*config.cells[1]*config.cells[2]),"gas grid dimensions changed without reset");check(std::isfinite(dt)&&dt>0,"invalid gas dt");
 const double h=config.h,v=h*h*h,area=h*h,c=std::sqrt(R*config.temperature_k),mu=config.viscosity();
 struct State{double rho,eps,p;Vec3 u;double sound;};std::vector<State> primitive(cells.size());
 GasReport report;double maxwave=0,minrho=1e100;
 for(std::size_t i=0;i<cells.size();++i)if(!cells[i].solid){auto& q=cells[i];check(q.mass>0&&std::isfinite(q.mass)&&q.epsilon>=.35&&q.epsilon<=1,"invalid gas state");auto u=velocity(int(i));primitive[i]={q.mass/(v*q.epsilon),q.epsilon,pressure(int(i)),u,c/std::sqrt(q.epsilon)};double speed=len(u);check(std::isfinite(speed)&&speed<=100,"gas speed exceeds low-Mach 100m/s contract");maxwave=std::max(maxwave,speed+c/std::sqrt(q.epsilon));minrho=std::min(minrho,q.mass/v);report.min_epsilon=std::min(report.min_epsilon,q.epsilon);report.max_speed=std::max(report.max_speed,speed);}
 report.cfl=3*maxwave*dt/h;check(report.cfl<=.4&&6*mu*dt/(minrho*h*h)<=.4,"gas acoustic/viscous CFL exceeded; reduce dt");
 auto next=cells;double boundary=0;
 auto state=[&](int i){return primitive[i];};
 auto face=[&](int l,int r,int k,int side){
  bool L=l>=0&&!cells[l].solid,B=r>=0&&!cells[r].solid;if(!L&&!B)return;
  State a=L?state(l):state(r),b=B?state(r):a;if(!L)a=b;
  bool wall=false;
  if(!L||!B){int missing=!L?l:r;double p=missing<0?config.boundary_pa[side]:0;State ghost=L?a:b;
   if(p==0){if(config.slip_walls)component(ghost.u,k,-axis(ghost.u,k));else ghost.u=times(ghost.u,-1);wall=true;}else{ghost.p=p;ghost.rho=p/(R*config.temperature_k);}
   if(!L)a=ghost;else b=ghost;
  }
  double eps=.5*(a.eps+b.eps),wave=std::max(std::abs(axis(a.u,k))+a.sound,std::abs(axis(b.u,k))+b.sound);
  double flux=eps*(.5*(a.rho*axis(a.u,k)+b.rho*axis(b.u,k))-.5*wave*(b.rho-a.rho));
  Vec3 mom=plus(times(a.u,.5*eps*a.rho*axis(a.u,k)),times(b.u,.5*eps*b.rho*axis(b.u,k)));
  mom=plus(mom,times(plus(times(b.u,b.rho),times(a.u,-a.rho)),-.5*wave*eps));component(mom,k,axis(mom,k)+.5*(a.p+b.p));
  mom=plus(mom,times(plus(b.u,times(a.u,-1)),-mu/h));
  if(wall)flux=0;
  if(L){next[l].mass-=dt*area*flux;next[l].momentum=plus(next[l].momentum,times(mom,-dt*area));}
  if(B){next[r].mass+=dt*area*flux;next[r].momentum=plus(next[r].momentum,times(mom,dt*area));}
  if(l<0)boundary-=dt*area*flux;if(r<0)boundary+=dt*area*flux;
 };
 for(int k=0;k<3;++k){auto n=config.cells;for(int z=0;z<n[2]+(k==2);++z)for(int y=0;y<n[1]+(k==1);++y)for(int x=0;x<n[0]+(k==0);++x){int q[3]{x,y,z},lo[3]{x,y,z};--lo[k];int l=lo[k]<0?-1:index(lo[0],lo[1],lo[2]),r=q[k]>=n[k]?-1:index(x,y,z);face(l,r,k,2*k+(r<0?1:0));}}
 for(auto& q:next)if(!q.solid){q.momentum=plus(q.momentum,times(gravity,q.mass*dt));check(std::isfinite(q.mass)&&q.mass>0&&std::isfinite(len(q.momentum)),"nonpositive/nonfinite gas update");}
 cells.swap(next);boundary_mass_out+=boundary;report.mass=mass();report.boundary_mass_out=boundary;return report;
}
double dense_drag_beta(double d,double rs,double rf,double mu,double eps,double speed){
 check(d>0&&rs>0&&rf>0&&mu>0&&eps>=.35&&eps<=1&&speed>=0&&std::isfinite(speed),"invalid dense drag state");
 double re=rf*d*speed/mu;check(re<=200000&&speed<=100,"dense drag Re/slip outside contract");
 if(eps<.8)return mu/(rs*eps*d*d)*(150*(1-eps)/eps+1.75*re);
 double er=eps*re,cdre=er<=1000?24*(1+.15*std::pow(er,.687)):.44*er;
 return .75*mu*cdre*std::pow(eps,-2.65)/(rs*eps*d*d);
}
CoupledWorld::CoupledWorld(GasConfig c):gas(c){}
void CoupledWorld::initialize(){minimum_step_s=0;gas.map_volume(dem.bodies);gas.initialize_pressure();initial_gas_mass=gas.mass();injected_mass=exited_mass=0;used_ids.clear();injected_by_type.clear();exited_by_type.clear();exited_by_face={};for(auto& b:dem.bodies){check(used_ids.insert(b.id).second,"duplicate coupled ID");injected_mass+=body_mass(b);injected_by_type[b.type]+=body_mass(b);}}
void CoupledWorld::inject(const std::vector<ClumpBody>& bs){
 auto next=dem.bodies;auto ids=used_ids;auto types=injected_by_type;double mass=0;
 for(auto& b:bs){check(ids.insert(b.id).second,"coupled ID cannot be reused");for(auto& p:clump_particles(b)){gas.stencil(p.position_m);for(const auto& old:next)for(const auto& q:clump_particles(old))check(len(plus(p.position_m,times(q.position_m,-1)))>=(p.diameter_m+q.diameter_m)/2,"injection overlap");}next.push_back(b);mass+=body_mass(b);types[b.type]+=body_mass(b);}
 auto g=gas;g.map_volume(next);dem.bodies=std::move(next);gas=std::move(g);used_ids=std::move(ids);injected_mass+=mass;injected_by_type=std::move(types);dem.invalidate_caches();
}
CoupledReport CoupledWorld::report()const{
 CoupledReport r;r.time=dem.physical_time_s;r.count=dem.bodies.size();r.gas_mass=gas.mass();r.gas_balance_error=r.gas_mass+gas.boundary_mass_out-initial_gas_mass;
 for(auto& b:dem.bodies){r.particle_mass+=body_mass(b);r.max_speed=std::max(r.max_speed,len(b.velocity_m_s));}
 r.particle_balance_error=r.particle_mass+exited_mass-injected_mass;r.exchange_error=exchange_error_;r.max_overlap=max_overlap_;for(auto& c:gas.cells)if(!c.solid)r.min_epsilon=std::min(r.min_epsilon,c.epsilon);return r;
}
CoupledReport CoupledWorld::step(double dt){
 using Clock=std::chrono::steady_clock;auto start=Clock::now();auto seconds=[](auto a,auto b){return std::chrono::duration<double>(b-a).count();};
 auto nextGas=gas;nextGas.step(dt,dem.gravity_m_s2);auto gasEnd=Clock::now();auto nextDem=dem;nextDem.external_wrenches.clear();
 Vec3 particleImpulse{},gasImpulse{};std::vector<Vec3> reaction(gas.cells.size());std::vector<double> rates(gas.cells.size());
 const double mu=gas.config.viscosity(),h=gas.config.h;auto n=gas.config.cells;
 // Each cell state/gradient is identical for every particle that gathers it.
 struct GatherCell {Vec3 u,grad;double rho=0,eps=1;};std::vector<GatherCell> gathered(nextGas.cells.size());
 for(std::size_t i=0;i<gathered.size();++i)if(!nextGas.cells[i].solid){auto& a=gathered[i];a.u=nextGas.velocity(int(i));a.rho=nextGas.pressure(int(i))/(R*gas.config.temperature_k);a.eps=nextGas.cells[i].epsilon;
  int q[3]{int(i)%n[0],(int(i)/n[0])%n[1],int(i)/(n[0]*n[1])};const double pc=nextGas.pressure(int(i));
  for(int k=0;k<3;++k){double pv[2];for(int side=0;side<2;++side){int c[3]{q[0],q[1],q[2]};c[k]+=side?1:-1;if(c[k]<0||c[k]>=n[k])pv[side]=gas.config.boundary_pa[2*k+side]>0?2*gas.config.boundary_pa[2*k+side]-pc:pc;else{int j=gas.index(c[0],c[1],c[2]);pv[side]=nextGas.cells[j].solid?pc:nextGas.pressure(j);}}component(a.grad,k,(pv[1]-pv[0])/(2*h));}
 }
 for(const auto& b:dem.bodies)for(const auto& p:clump_particles(b)){
  auto e=gas.extent();if(p.position_m.x<0||p.position_m.x>e.x||p.position_m.y<0||p.position_m.y>e.y||p.position_m.z<0||p.position_m.z>e.z)continue;
  auto weights=nextGas.stencil(p.position_m);Vec3 u{},grad{};double rho=0,eps=0;
  for(auto w:weights){const auto& a=gathered[w.first];u=plus(u,times(a.u,w.second));rho+=w.second*a.rho;eps+=w.second*a.eps;grad=plus(grad,times(a.grad,w.second));}
  Vec3 slip=plus(u,times(p.velocity_m_s,-1));double vp=volume(p),mass=vp*b.density_kg_m3,beta=dense_drag_beta(p.diameter_m,b.density_kg_m3,rho,mu,eps,len(slip));
  check(beta*dt<=.1,"particle drag exchange unresolved; reduce dt");Vec3 force=plus(times(slip,mass*beta),times(grad,-vp));
  auto& load=nextDem.external_wrenches[b.id];load.force_n=plus(load.force_n,force);load.torque_nm=plus(load.torque_nm,cross3(plus(p.position_m,times(b.position_m,-1)),force));
  particleImpulse=plus(particleImpulse,times(force,dt));
  for(auto w:weights){reaction[w.first]=plus(reaction[w.first],times(force,-w.second));rates[w.first]+=w.second*mass*beta;}
 }
 if(two_way)for(std::size_t i=0;i<reaction.size();++i)if(!nextGas.cells[i].solid){check(dt*rates[i]/nextGas.cells[i].mass<=.1,"gas drag exchange unresolved; reduce dt");Vec3 impulse=times(reaction[i],dt);nextGas.cells[i].momentum=plus(nextGas.cells[i].momentum,impulse);gasImpulse=plus(gasImpulse,impulse);}
 for(std::size_t i=0;i<nextGas.cells.size();++i)if(!nextGas.cells[i].solid)check(std::isfinite(len(nextGas.velocity(int(i))))&&len(nextGas.velocity(int(i)))<=100,"post-exchange gas state exceeds contract");
 auto forcesEnd=Clock::now();auto contact=nextDem.step(dt);auto demEnd=Clock::now();double exited=0;auto faces=exited_by_face;auto exitTypes=exited_by_type;const auto extent=gas.extent();std::vector<ClumpBody> remaining;
 for(auto& b:nextDem.bodies){int face=-1;for(int k=0;k<3;++k){if(axis(b.position_m,k)<0){check(gas.config.boundary_pa[2*k]>0,"particle crossed closed boundary");if(face<0)face=2*k;}if(axis(b.position_m,k)>axis(extent,k)){check(gas.config.boundary_pa[2*k+1]>0,"particle crossed closed boundary");if(face<0)face=2*k+1;}}if(face<0)remaining.push_back(b);else{check(gas.config.boundary_pa[face]>0,"particle crossed closed boundary");double m=body_mass(b);exited+=m;faces[face]+=m;exitTypes[b.type]+=m;}}
 if(remaining.size()!=nextDem.bodies.size()){nextDem.bodies=std::move(remaining);nextDem.invalidate_caches();}
 nextGas.map_volume(nextDem.bodies);check(std::abs(nextGas.mass()+nextGas.boundary_mass_out-initial_gas_mass)<=std::max(1e-12,1e-8*initial_gas_mass),"gas mass ledger mismatch");gas=std::move(nextGas);dem=std::move(nextDem);exited_mass+=exited;exited_by_face=faces;exited_by_type=std::move(exitTypes);exchange_error_=len(plus(particleImpulse,gasImpulse));max_overlap_=contact.max_overlap_m;minimum_step_s=minimum_step_s==0?dt:std::min(minimum_step_s,dt);timing.gas_s+=seconds(start,gasEnd);timing.coupling_s+=seconds(gasEnd,forcesEnd);timing.dem_s+=seconds(forcesEnd,demEnd);timing.commit_s+=seconds(demEnd,Clock::now());return report();
}
void add_coupled_obstacle(CoupledWorld& w,std::array<int,3> first,std::array<int,3> end){
 for(int k=0;k<3;++k)check(first[k]>=0&&first[k]<end[k]&&end[k]<=w.gas.config.cells[k],"invalid obstacle cell range");
 double h=w.gas.config.h;FixedBox b;b.id=w.dem.boxes.size()+1;b.centre_m={(first[0]+end[0])*h/2,(first[1]+end[1])*h/2,(first[2]+end[2])*h/2};b.half_m={(end[0]-first[0])*h/2,(end[1]-first[1])*h/2,(end[2]-first[2])*h/2};w.dem.boxes.push_back(b);
 for(int z=first[2];z<end[2];++z)for(int y=first[1];y<end[1];++y)for(int x=first[0];x<end[0];++x)w.gas.cells[w.gas.index(x,y,z)].solid=true;
}
CoupledWorld coupled_demo(unsigned count,double drop,double gamma,double temperature,bool obstacle,int flow_axis){
 check(count>=1&&count<=512,"coupled demo supports 1..512 bodies");GasConfig g;check(flow_axis>=0&&flow_axis<3,"flow axis must be 0..2");g.temperature_k=temperature;g.boundary_pa={};g.boundary_pa[2*flow_axis]=g.initial_pressure_pa;g.boundary_pa[2*flow_axis+1]=g.initial_pressure_pa-drop;CoupledWorld w(g);
 w.dem.maximum_bodies=1024;w.dem.maximum_pebbles=2048;
 for(unsigned type=0;type<3;++type){MaterialContact m;m.law=ContactLaw::Linear;m.linear.kn=10;m.linear.kt=2.5;m.linear.cn=.0002;m.linear.ct=.0001;m.linear.mu=.4;m.rolling.mu=.02;m.rolling.stiffness_n_m=.1;m.rolling.damping_ns_m=1e-6;m.adhesion_gamma_j_m2=gamma;w.dem.wall_models[type]=m;for(unsigned other=type;other<3;++other)w.dem.pair_models[{type,other}]=m;}
 auto e=w.gas.extent();for(int k=0;k<3;++k)if(k!=flow_axis)for(int side=0;side<2;++side){Vec3 normal{};component(normal,k,side?-1:1);w.dem.planes.push_back({static_cast<std::uint64_t>(2*k+side+1),normal,side?-axis(e,k):0});}
 for(unsigned i=0;i<count;++i){unsigned type=i%3;double d=type==0?.003:type==1?.004:.003;std::vector<Pebble> pebbles{{{},d}};if(type==2)pebbles={{{-.0015,0,0},d},{{.0015,0,0},d}};auto b=make_clump(i+1,type,pebbles,type==0?900:type==1?1500:2400);b.position_m={.025+(i%8)*.009,.025+((i/8)%8)*.009,.025+(i/64)*.009};w.dem.bodies.push_back(b);}
 if(obstacle)add_coupled_obstacle(w,{9,0,0},{10,5,6});
 w.initialize();return w;
}
CoupledWorld coupled_experiment(int scenario){
 check(scenario>=1&&scenario<=6,"experiment must be 1..6");bool filter=scenario==2||scenario==5;
 GasConfig c;c.cells={4,4,filter?8:4};c.h=.005;c.boundary_pa={};int flow=filter?2:0;c.boundary_pa[2*flow]=c.boundary_pa[2*flow+1]=101325;
 CoupledWorld w(c);w.dem=coupled_demo(1,0).dem;w.dem.bodies.clear();w.dem.planes.clear();
 for(auto& a:w.dem.wall_models){a.second.adhesion_gamma_j_m2=scenario==5?0:.002;a.second.linear.cn=filter?.02:.003;}
 for(auto& a:w.dem.pair_models){a.second.adhesion_gamma_j_m2=.002;a.second.linear.cn=filter?.02:.003;}
 {for(auto& a:w.dem.wall_models){a.second.linear.kn=filter?1000:100;a.second.linear.kt=filter?250:25;}for(auto& a:w.dem.pair_models){a.second.linear.kn=filter?1000:100;a.second.linear.kt=filter?250:25;}}
 const auto e=w.gas.extent();for(int axis=0;axis<3;++axis)if(axis!=flow)for(int side=0;side<2;++side){Vec3 n{};component(n,axis,side?-1:1);w.dem.planes.push_back({std::uint64_t(2*axis+side+1),n,side?-(axis==0?e.x:axis==1?e.y:e.z):0});}
 if(filter){w.dem.planes.push_back({7,{0,0,1},0});w.dem.planes.push_back({8,{0,0,-1},-.01});}
 for(unsigned i=0;i<3;++i){std::vector<Pebble> shape{{{},.001}};if(i==2)shape={{{-.0005,0,0},.001},{{.0005,0,0},.001}};
  auto b=make_clump(i+1,i,shape,1000+i*100);b.position_m=filter?Vec3{.004+i*.006,.01,.0005}:Vec3{.003,.004+i*.006,.008};
  if(filter){double pebbleMass=pi/6*1e-9*b.density_kg_m3,pull=4*pi*w.dem.wall_models[i].adhesion_gamma_j_m2*.0005;b.position_m.z-=(pebbleMass*9.81+pull)/w.dem.wall_models[i].linear.kn;}
  w.dem.bodies.push_back(b);
 }
 if(scenario==3)add_coupled_obstacle(w,{2,0,0},{3,4,4});if(scenario==6)add_coupled_obstacle(w,{2,0,0},{3,2,4});w.initialize();return w;
}
const char* coupled_experiment_stage(int scenario,double t){
 if(scenario==2||scenario==5)return t<.005?"settled":t<.06?"resuspend/capture":t<.16?"pressure release":"retention after air off";
 return t<.005?"initial":t<.015?"pressure transport":t<.08?"second pulse / transport":"residual motion after supply stop";
}
CoupledReport step_coupled_experiment(CoupledWorld& w,int scenario,double dt){
 check(scenario>=1&&scenario<=6&&dt>0&&dt<=2e-7,"experiment requires dt <=2e-7s");double t=w.dem.physical_time_s;
 if(scenario==2||scenario==5){double drop=t<.005?0:t<.06?2000:t<.16?2000*(.16-t)/.1:0;w.gas.config.boundary_pa[5]=101325-drop;}
 else{
  w.gas.config.boundary_pa[1]=101325-(scenario==4||t<.005?0:2000);
  if(t>=.015&&!w.used_ids.count(1000)){std::vector<ClumpBody> pulse;for(unsigned i=0;i<3;++i){std::vector<Pebble> shape{{{},.001}};if(i==2)shape={{{-.0005,0,0},.001},{{.0005,0,0},.001}};auto b=make_clump(1000+i,i,shape,1000+i*100);b.position_m={.003,.004+i*.006,.016};pulse.push_back(b);}w.inject(pulse);}
 }
 // Transactional retries only for the identified contact stability limit.
 // Subdivide time; do not weaken stiffness, forces, body count, or rejection guards.
 double remaining=dt,trial=dt;CoupledReport result;
 while(remaining>dt*1e-10){trial=std::min(trial,remaining);try{result=w.step(trial);check(result.max_overlap<.00005,"experiment overlap exceeds 5% diameter; reset with resolved contact properties");remaining-=trial;}
 catch(const std::invalid_argument& e){if(std::string(e.what())!="dt exceeds rigid clump multi-contact limit"||trial<=1e-11)throw;trial*=.5;}}
 return result;
}
double coupled_filter_mass(const CoupledWorld& w){double mass=0;for(const auto& b:w.dem.bodies){bool touching=false;for(const auto& p:clump_particles(b))if(p.position_m.z+p.diameter_m/2>=.01-1e-6)touching=true;if(touching)mass+=body_mass(b);}return mass;}

}
