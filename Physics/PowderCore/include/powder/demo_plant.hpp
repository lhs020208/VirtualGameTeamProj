#pragma once
#include "plant.hpp"
#include <numeric>
// Deliberately approximate, interactive presentation model. Not DEM.
// IDs/species never change; every displayed grain remains in the state vector.
namespace powder {
namespace demo {
struct V {float x=0,y=0,z=0;};
inline V operator+(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V operator-(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V operator*(V a,float b){return {a.x*b,a.y*b,a.z*b};}
inline float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline float length(V a){return std::sqrt(dot(a,a));}
inline V xyz(Grain p){return {p.x,p.y,p.z};}
inline V rotate(V p,float a){float s=std::sin(a),c=std::cos(a);return {p.x,c*p.y-s*p.z,s*p.y+c*p.z};}
inline unsigned hash(int x,int y,int z){return (unsigned(x)*73856093u^unsigned(y)*19349663u^unsigned(z)*83492791u)&65535u;}
struct Segment {V a,b,t;float radius,length,start;int data;};
struct Route {std::vector<Segment> cells;float length=0,gate=0;};
struct Box {V center,axis[3],half;};
}
class DemoPlant {
 using V=demo::V;
 std::vector<Grain> pos,field;
 std::vector<V> vel,old,correction,offset;
 std::vector<int> zone,link,cx,cy,cz;
 std::vector<float> distance;
 std::vector<demo::Route> routes;
 std::vector<demo::Box> boxes;
 std::vector<int> gates,head;
 float radius,dt;double seconds=0,theta=0;
 unsigned iterations;
 unsigned groupSize=8;float displayRadius;std::vector<float> species;
 int whgate,rhgate;
 V pivot;
 float rotorRadius=0;
 std::array<double,9> lastArrival{};
 std::array<bool,9> sleeping{};
 std::uint64_t invalid=0;
 float gate_limit(int z)const{return z<6?field[gates[z]].z:z==6?field[whgate].z:field[rhgate].z;}
 bool opened(int z)const{return field[(z<6?gates[z]:z==6?whgate:rhgate)+1].y>0;}
 static float radial(float z,float bottom,float neck,float shoulder,float wide){return std::clamp(neck+(wide-neck)*(z-bottom)/shoulder,neck,wide);}
 V contain(V p,int z,bool close_floor=true)const{
  if(z<=7){V center=z<6?demo::xyz(field[gates[z]]):z==6?V{}:V{3,0,0};
   float floor=gate_limit(z),roof=z<6?3.3f:z==6?1.8f:3.38f;
   float neck=z<7?.022f:.136f,wide=z<7?.35f:.25f;
   float coneBottom=z<6?2.3f:z==6?.7f:2.27f,coneHeight=z<6?.3f:z==6?.25f:.36f;
   if(close_floor)p.z=std::max(p.z,floor+radius);
   p.z=std::min(p.z,roof-radius);
   float limit=std::max(.002f,radial(p.z,coneBottom,neck,coneHeight,wide)-radius*1.6f);
   V q=p-center;float r=std::hypot(q.x,q.y);if(r>limit){p.x=center.x+q.x*limit/r;p.y=center.y+q.y*limit/r;}
  }else if(z==8){
   p.x=std::clamp(p.x,2.625f+radius,3.375f-radius);p.z=std::min(p.z,1.2675f-radius);
   float limit=.225f-radius;V q=p-pivot;
   if(q.z<0){float r=std::hypot(q.y/limit,q.z/(.1625f-radius));if(r>1){p.y=pivot.y+q.y/r;p.z=pivot.z+q.z/r;}}
   else p.y=std::clamp(p.y,-limit,limit);
  }
  return p;
 }
 V rotor(V p)const{
  // Conservative swept envelope of the actual rotating mesh. The circulation
  // field supplies stirring; individual paddle impacts are intentionally omitted.
  if(p.x>2.58f-radius&&p.x<3.42f+radius){V q=p-pivot;float r=std::hypot(q.y,q.z),limit=rotorRadius+radius;if(r<limit){if(r<1e-8f){q.y=0;q.z=-limit;}else{q.y*=limit/r;q.z*=limit/r;}p.y=pivot.y+q.y;p.z=pivot.z+q.z;}}
  return p;
 }
 float closest_distance(int route,V p)const{
  float best=1e30f,s=0;for(auto&c:routes[route].cells){float t=std::clamp(demo::dot(p-c.a,c.t),0.f,c.length);float d=demo::dot(p-(c.a+c.t*t),p-(c.a+c.t*t));if(d<best){best=d;s=c.start+t;}}return s;
 }
 V route_point(int route,float s,unsigned id)const{
  const auto&r=routes[route];auto it=std::upper_bound(r.cells.begin(),r.cells.end(),s,[](float x,const demo::Segment&c){return x<c.start;});if(it!=r.cells.begin())--it;
  V lateral=offset.empty()?V{}:offset[id]-it->t*demo::dot(offset[id],it->t);
  float len=demo::length(lateral),limit=std::max(0.f,it->radius-radius*2);if(len>limit)lateral=lateral*(limit/len);
  return it->a+it->t*std::clamp(s-it->start,0.f,it->length)+lateral;
 }
public:
 DemoPlant(const GranularConfig&c,const std::vector<Grain>&p,const std::vector<PlantRoute>&rs,const std::string&boxfile):field(c.plant_data),radius(c.radius*2),dt(c.dt),iterations(c.iterations),displayRadius(c.radius),pivot(demo::xyz(field[1])){
  if(p.empty()||!std::isfinite(dt)||dt<=0||dt>1.f/60||iterations>4||!std::isfinite(radius)||radius<=0)throw std::invalid_argument("invalid demo configuration");
  gates=plant_supplier_gates(field);whgate=plant_weighing_gate(field);rhgate=plant_receiver_gate(field);
  if(gates.size()!=6||rs.size()!=7)throw std::invalid_argument("demo requires current six-supplier plant topology");
  for(unsigned j=0;j<rs.size();++j){demo::Route r;for(int k:rs[j].cells){V a=demo::xyz(field[k]),b=demo::xyz(field[k+1]);float l=demo::length(b-a);r.cells.push_back({a,b,(b-a)*(1/l),std::min(field[k].w,field[k+1].w),l,r.length,k});r.length+=l;}routes.push_back(r);routes.back().gate=closest_distance(j,demo::xyz(field[j<6?gates[j]:whgate]));}
  std::ifstream f(boxfile);int n;if(!(f>>n)||n<1||n>1000)throw std::runtime_error("demo rotor proxies missing");for(int i=0;i<n;++i){demo::Box b;f>>b.center.x>>b.center.y>>b.center.z;for(auto&a:b.axis)f>>a.x>>a.y>>a.z;f>>b.half.x>>b.half.y>>b.half.z;if(!f)throw std::runtime_error("invalid demo rotor proxy");boxes.push_back(b);}
  if(!(f>>rotorRadius)||rotorRadius<=0||rotorRadius>.15f)throw std::runtime_error("invalid demo rotor swept radius");
  if(p.size()%groupSize)throw std::invalid_argument("demo groups require a multiple of eight particles");
  for(unsigned i=0;i<p.size();i+=groupSize){V mean{};for(unsigned k=0;k<groupSize;++k){if(p[i+k].w!=p[i].w)throw std::invalid_argument("demo group crosses species boundary");mean=mean+demo::xyz(p[i+k]);species.push_back(p[i+k].w);}mean=mean*(1.f/groupSize);pos.push_back({mean.x,mean.y,mean.z,p[i].w});}
  auto count=pos.size();vel.resize(count);old.resize(count);correction.resize(count);offset.resize(count);zone.resize(count);link.resize(count);cx.resize(count);cy.resize(count);cz.resize(count);distance.resize(count);head.resize(65536);
  for(unsigned i=0;i<count;++i){float best=1e9;for(int j=0;j<6;++j){V q=demo::xyz(pos[i])-demo::xyz(field[gates[j]]);float d=q.x*q.x+q.y*q.y;if(d<best){best=d;zone[i]=j;}}}
 }
 void update_plant_field(const std::vector<Grain>&d){for(auto g:gates)field[g+1]=d[g+1];field[whgate+1]=d[whgate+1];field[rhgate+1]=d[rhgate+1];for(auto&r:routes)for(auto&c:r.cells)field[c.data+2]=d[c.data+2];}
 void advance(unsigned steps,float rpm,bool){
  if(!std::isfinite(rpm)||rpm<0||rpm>30)throw std::invalid_argument("invalid demo RPM");
  for(unsigned step=0;step<steps;++step){theta+=rpm*.10471975511966*dt;float damping=std::exp(-3.f*dt);
   // Presentation-only bulk resting state, not a convergence claim. Retain
   // every grain and its volume; arrivals and effective discharge wake a zone.
   for(int z=0;z<9;++z){bool discharge=z==8?rpm>0:z==7?opened(z):opened(z)&&field[routes[z].cells.front().data+2].x>1e-8f;sleeping[z]=!discharge&&seconds-lastArrival[z]>.5;}
   for(unsigned i=0;i<pos.size();++i){V p=demo::xyz(pos[i]);old[i]=p;int z=zone[i];
    if(z<=8&&sleeping[z]){vel[i]={};continue;}
    if(z>=10){int r=z-10;auto&route=routes[r];auto&c=route.cells.front();float flow=field[c.data+2].x;float speed=std::clamp(flow/(3.14159265f*.022f*.022f)*.18f,-3.f,3.f);
     distance[i]=std::clamp(distance[i]+speed*dt,route.gate-radius,route.length);p=route_point(r,distance[i],i);
     if(distance[i]>=route.length-1e-6f&&speed>0){zone[i]=r<6?6:7;lastArrival[zone[i]]=seconds;sleeping[zone[i]]=false;vel[i]={0,0,-.1f};}
    }else if(z==9){vel[i].z-=9.81f*dt;p=p+vel[i]*dt;float bore=radial(p.z,1.35f,.022f,.335f,.136f)-radius*1.6f;float radialLength=std::hypot(p.x-3,p.y);if(radialLength>bore){p.x=3+(p.x-3)*bore/radialLength;p.y*=bore/radialLength;}if(p.z<=1.2675f-radius){zone[i]=8;lastArrival[8]=seconds;sleeping[8]=false;p=contain(p,8);}}
    else{
     vel[i]=vel[i]*damping;vel[i].z-=9.81f*dt;
     if(z==8&&rpm>0){V q=p-pivot;float omega=rpm*.104719755f;V target{.07f*omega*std::sin(float(seconds)*.7f+q.x*5),-omega*q.z*.6f,omega*q.y*.6f};vel[i]=vel[i]+(target-vel[i])*std::min(1.f,dt*8);}
     float speed=demo::length(vel[i]);if(speed>2)vel[i]=vel[i]*(2/speed);p=p+vel[i]*dt;
     p=contain(p,z);if(z==8)p=contain(rotor(p),z);
     if(z<=7&&opened(z)&&p.z<=gate_limit(z)+radius*2){V center=demo::xyz(field[z<6?gates[z]:z==6?whgate:rhgate]);float rad=z<7?.019f:.13f;
      if(std::hypot(p.x-center.x,p.y-center.y)<rad){if(z<7){float flow=field[routes[z].cells.front().data+2].x;if(flow>1e-8f){zone[i]=10+z;distance[i]=closest_distance(z,p);offset[i]={};offset[i]=p-route_point(z,distance[i],i);}}
       else{zone[i]=9;vel[i]={0,0,-.1f};}}
     }
    }
    pos[i]={p.x,p.y,p.z,pos[i].w};
   }
   for(unsigned it=0;it<iterations;++it){std::fill(head.begin(),head.end(),-1);float cell=2*radius;
    for(unsigned i=0;i<pos.size();++i)if(zone[i]<=8&&!sleeping[zone[i]]){V p=demo::xyz(pos[i]);cx[i]=int(std::floor(p.x/cell));cy[i]=int(std::floor(p.y/cell));cz[i]=int(std::floor(p.z/cell));unsigned h=demo::hash(cx[i],cy[i],cz[i]);link[i]=head[h];head[h]=int(i);}
    for(unsigned i=0;i<pos.size();++i){correction[i]={};if(zone[i]>8||sleeping[zone[i]])continue;V p=demo::xyz(pos[i]),sum{};int contacts=0;
     for(int z=-1;z<=1;++z)for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){int xx=cx[i]+x,yy=cy[i]+y,zz=cz[i]+z;for(int j=head[demo::hash(xx,yy,zz)];j>=0;j=link[j])if(j!=int(i)&&zone[j]==zone[i]&&cx[j]==xx&&cy[j]==yy&&cz[j]==zz){V d=p-demo::xyz(pos[j]);float d2=demo::dot(d,d);if(d2<cell*cell){float l=std::sqrt(d2);V n=l>1e-8f?d*(1/l):V{i>unsigned(j)?1.f:-1.f,0,0};sum=sum+n*((cell-l)*.45f);++contacts;}}}
     if(contacts>4)sum=sum*(4.f/contacts);float len=demo::length(sum);if(len>radius*.5f)sum=sum*(radius*.5f/len);correction[i]=sum;
    }
    for(unsigned i=0;i<pos.size();++i)if(zone[i]<=8&&!sleeping[zone[i]]){V p=contain(demo::xyz(pos[i])+correction[i],zone[i]);if(zone[i]==8)p=contain(rotor(p),8);pos[i]={p.x,p.y,p.z,pos[i].w};}
   }
   for(unsigned i=0;i<pos.size();++i){V p=demo::xyz(pos[i]);if(zone[i]<=8){vel[i]=(p-old[i])*(1/dt);float speed=demo::length(vel[i]);if(speed>2)vel[i]=vel[i]*(2/speed);}if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)){++invalid;throw std::runtime_error("nonfinite demo particle");}}
   seconds+=dt;
  }
 }
 std::vector<Grain> snapshot()const{std::vector<Grain> out;out.reserve(species.size());for(unsigned i=0;i<pos.size();++i)for(unsigned k=0;k<groupSize;++k){V visualOffset{(k&1?1.f:-1.f)*displayRadius,(k&2?1.f:-1.f)*displayRadius,(k&4?1.f:-1.f)*displayRadius};V p=demo::xyz(pos[i])+visualOffset;if(zone[i]<=8)p=contain(p,zone[i]);out.push_back({p.x,p.y,p.z,species[out.size()]});}return out;}
 double time()const{return seconds;}
 float angle()const{return float(theta);}
 std::string device()const{return "CPU demo: constrained routes + soft particle contacts (not DEM)";}
 std::array<unsigned,10> counts()const{std::array<unsigned,10> out{};for(int z:zone)out[z>=10?9:z]+=groupSize;return out;}
 double max_containment_error()const{double worst=0;for(unsigned i=0;i<pos.size();++i)if(zone[i]<=8)worst=std::max(worst,double(demo::length(demo::xyz(pos[i])-contain(demo::xyz(pos[i]),zone[i]))));return worst;}
};
}
