#pragma once
#include "powder/plant.hpp"
namespace powder {
struct PlantCharge { std::vector<Grain> particles;std::vector<int> suppliers; };
inline PlantCharge resize_plant_charge(const std::vector<Grain>& data,const std::vector<PlantRoute>& routes,
 const std::vector<Grain>& original,const std::vector<int>& suppliers,const std::vector<int>& counts,float default_radius,const std::array<float,6>* supplier_radii=nullptr){
 const auto gates=plant_supplier_gates(data);
 if(counts.size()!=gates.size()||original.size()!=suppliers.size()||!std::isfinite(default_radius)||default_radius<=0)throw std::invalid_argument("particle count settings do not match plant");
 size_t total=0;for(int n:counts){if(n<0||n>100000)throw std::invalid_argument("each supplier count must be 0..100000");total+=n;}
 PlantCharge out;out.particles.reserve(total);out.suppliers.reserve(total);
 std::vector<const PlantRoute*> feeds;for(const auto& r:routes)if(r.phase==0)feeds.push_back(&r);
 if(feeds.size()!=counts.size())throw std::invalid_argument("supplier route count mismatch");
 for(size_t j=0;j<counts.size();++j){
  if(counts[j]==0)continue;
  const float radius=supplier_radii?supplier_radii->at(j):default_radius;
  if(!std::isfinite(radius)||radius<=0)throw std::invalid_argument("invalid supplier radius");
  std::vector<Grain> old;for(size_t i=0;i<original.size();++i)if(suppliers[i]==int(j))old.push_back(original[i]);
  if(size_t(counts[j])<=old.size()){for(int n=0;n<counts[j];++n){out.particles.push_back(old[n]);out.suppliers.push_back(int(j));}continue;}
  // Fill the known vertical barrel, not arbitrary world-space copies or overlap.
  Grain top{},bottom{};double best=0;auto gate=data[gates[j]];
  for(int at:feeds[j]->cells){auto a=data[at],b=data[at+1],m=data[at+2];double R=std::min(a.w,b.w);
   if(m.z!=0||m.w!=0||a.z<=b.z||b.z<=gate.z||std::hypot(a.x-b.x,a.y-b.y)>1.e-5||std::abs(a.w-b.w)>1.e-5)continue;
   double volume=R*R*(a.z-b.z);if(volume>best){best=volume;top=a;bottom=b;}
  }
  double usable=.95*std::min(top.w,bottom.w)-radius,pitch=2.1*radius;
  if(best<=0||usable<=radius)throw std::invalid_argument("no supported supplier barrel for initial particles");
  const int extent=int(std::floor(usable/pitch)),layers=int(std::floor((top.z-bottom.z-2.2*radius)/pitch))+1;
  std::vector<std::pair<int,int>> xy;for(int y=-extent;y<=extent;++y)for(int x=-extent;x<=extent;++x)if(std::hypot(x*pitch,y*pitch)<=usable)xy.emplace_back(x,y);
  std::stable_sort(xy.begin(),xy.end(),[](auto a,auto b){return a.first*a.first+a.second*a.second<b.first*b.first+b.second*b.second;});
  if(layers<=0||size_t(layers)*xy.size()<size_t(counts[j]))throw std::invalid_argument("requested particle count exceeds supplier barrel capacity");
  float species=old.empty()?float(j):old.front().w;
  for(int n=0;n<counts[j];++n){auto cell=xy[size_t(n)%xy.size()];int layer=int(size_t(n)/xy.size());
   out.particles.push_back({float(top.x+cell.first*pitch),float(top.y+cell.second*pitch),float(bottom.z+radius*1.1+layer*pitch),species});out.suppliers.push_back(int(j));
  }
 }
 return out;
}
}
