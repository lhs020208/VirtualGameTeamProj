#pragma once
#include "granular.hpp"
#include <cmath>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <algorithm>
#include <set>
namespace powder {
// Packed source-mesh BVHs, route cells and valve locations. SI, Blender XYZ.
inline void validate_plant_data(const std::vector<Grain>& d){
 auto need=[](bool ok){if(!ok)throw std::invalid_argument("invalid plant geometry/flow data");};
 need(d.size()>=16&&d.size()<4000000);
 for(auto a:d)for(float v:{a.x,a.y,a.z,a.w})need(std::isfinite(v));
 auto index=[&](float f){need(f>=0&&f<d.size()&&std::floor(f)==f);return int(f);};
 need(d[1].w>=100&&d[1].w<=10000);need(d[2].x<d[3].x&&d[2].y<d[3].y&&d[2].z<d[3].z);
 need(d[4].x>=0&&d[4].x<=2&&std::floor(d[4].x)==d[4].x);
 std::set<int> visited;
 std::function<void(int,int)> walk=[&](int at,int depth){need(at>=16&&at+1<d.size()&&depth<60&&visited.insert(at).second);auto lo=d[at],hi=d[at+1];need(lo.x<=hi.x&&lo.y<=hi.y&&lo.z<=hi.z);
  if(lo.w>=0){walk(index(lo.w),depth+1);walk(index(hi.w),depth+1);}
  else {need(-lo.w<=16&&std::floor(lo.w)==lo.w);int start=index(hi.w);need(start>=16&&start+int(-lo.w)*3<=d.size());for(int j=0;j<int(-lo.w);++j){auto a=d[start+3*j],b=d[start+3*j+1],c=d[start+3*j+2];double x=(b.y-a.y)*(c.z-a.z)-(b.z-a.z)*(c.y-a.y),y=(b.z-a.z)*(c.x-a.x)-(b.x-a.x)*(c.z-a.z),z=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);need(x*x+y*y+z*z>1e-24);}}
 };
 for(float root:{d[0].x,d[0].y})if(root!=-1)walk(index(root),0);
 int start=index(d[0].z),count=int(d[0].w);need(count>=0&&count==d[0].w&&start+3*count<=d.size());
 for(int j=0;j<count;++j){auto a=d[start+3*j],b=d[start+3*j+1],q=d[start+3*j+2];need(a.w>.001&&b.w>.001&&a.w<2&&b.w<2);need(std::hypot(std::hypot(b.x-a.x,b.y-a.y),b.z-a.z)>1e-7);need(q.w>=0&&q.w<=2&&std::floor(q.w)==q.w);need(std::abs(q.x)/(3.141592653589793*std::pow(std::min(a.w,b.w),2))<=80);}
 int gates=index(d[5].x),n=int(d[5].y);need(n>=0&&n==d[5].y&&gates+2*n<=d.size());for(int i=0;i<n;++i){need(d[gates+2*i].w>0);float phase=d[gates+2*i+1].x;need(phase>=0&&phase<=2&&std::floor(phase)==phase);}
 int grid=index(d[6].x),size=1;for(float v:{d[6].y,d[6].z,d[6].w}){need(v>=1&&v<=256&&std::floor(v)==v);size*=int(v);}need(size<=1000000&&grid+size<=d.size()&&d[7].w>0);
 for(int i=grid;i<grid+size;++i){int entries=int(d[i].y);need(entries>=0&&entries==d[i].y);if(!entries)continue;int at=index(d[i].x);need(at+entries<=d.size());for(int j=0;j<entries;++j){int cell=index(d[at+j].x);need(cell>=start&&cell<start+3*int(d[0].w)&&(cell-start)%3==0);}}
}
inline std::vector<Grain> read_plant_data(const std::string& path){std::ifstream f(path,std::ios::binary);std::uint32_t n=0;f.read(reinterpret_cast<char*>(&n),4);if(!f||n<16||n>=4000000)throw std::runtime_error("plant file header invalid");std::vector<Grain>d(n);f.read(reinterpret_cast<char*>(d.data()),n*sizeof(Grain));if(!f||f.peek()!=EOF)throw std::runtime_error("plant file length invalid");validate_plant_data(d);return d;}
// Incompressible, quasi-steady 1D pipe cells: Darcy-Weisbach + minor losses.
// One-way dilute carrier gas only. No resolved turbulence or powder pressure feedback.
inline double plant_cell_loss(double q,double length,double radius,double minor=0,double area_override=0){
 if(!std::isfinite(q)||!std::isfinite(length)||!std::isfinite(radius)||length<=0||radius<=0||minor<0)throw std::invalid_argument("invalid pipe cell");
 double u=std::abs(q)/(area_override>0?area_override:3.141592653589793*radius*radius),re=1.2*u*2*radius/1.81e-5;
 if(re==0)return 0;double lam=64/re,turb=.3164/std::pow(std::max(re,1.),.25),w=std::clamp((re-2300)/1700.,0.,1.);
 double loss=((lam*(1-w)+turb*w)*length/(2*radius)+minor)*.5*1.2*u*u;return std::copysign(loss,q);
}
// Test valve schedule, not a mass-loss tolerance: retained grains stay simulated.
inline bool plant_discharge_ready(int received,int total,double elapsed,double fraction=.98,double wait=2.){
 if(total<=0||received<0||received>total||!std::isfinite(elapsed)||elapsed<0||!std::isfinite(fraction)||fraction<=0||fraction>1||!std::isfinite(wait)||wait<0)throw std::invalid_argument("invalid plant discharge schedule");
 return received==total||(elapsed>=wait&&double(received)/total>=fraction);
}
struct PlantRoute {int phase=0;std::vector<int> cells;};
inline std::vector<PlantRoute> read_plant_routes(const std::string& path){std::ifstream f(path);int n; if(!(f>>n)||n<1||n>32)throw std::runtime_error("invalid route file");std::vector<PlantRoute> routes(n);for(auto&r:routes){int count;if(!(f>>r.phase>>count)||r.phase<0||r.phase>2||count<1||count>10000)throw std::runtime_error("invalid route header");r.cells.resize(count);for(auto&c:r.cells)if(!(f>>c))throw std::runtime_error("invalid route cell");}return routes;}
inline void solve_plant_pressure(std::vector<Grain>&d,const std::vector<PlantRoute>& routes,double drop){
 if(!std::isfinite(drop)||drop<0||drop>3000)throw std::invalid_argument("test pressure drop must be 0..3000 Pa");
 for(const auto&r:routes){double hi=1;auto loss=[&](double q){double sum=0;for(int i:r.cells){if(i<int(d[0].z)||i+2>=d.size()||(i-int(d[0].z))%3||d[i+2].w!=r.phase)throw std::invalid_argument("route index mismatch");auto a=d[i],b=d[i+1];double l=std::hypot(std::hypot(b.x-a.x,b.y-a.y),b.z-a.z),rad=std::min(a.w,b.w);if(d[i+2].z==1)sum+=plant_cell_loss(q,l,2*a.w*b.w/(a.w+b.w),d[i+2].y,4*a.w*b.w);else sum+=plant_cell_loss(q,l,rad,d[i+2].y);}return sum;};double lo=0;
  for(int k=0;k<80;++k){double q=(lo+hi)/2;if(loss(q)>drop)hi=q;else lo=q;}
  double q=(lo+hi)/2;for(int i:r.cells)d[i+2].x=float(q);
 }validate_plant_data(d);
}
}
