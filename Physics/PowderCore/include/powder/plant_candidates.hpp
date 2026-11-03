#pragma once
#include "granular.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>
#include <stdexcept>
namespace powder {
// GPU-only acceleration. Never changes asset topology, shape arithmetic or rank.
inline void plant_candidate_upload(std::vector<Grain>& d,unsigned mode){
 if(mode<4||d[10].x!=2)return;
 struct Item {Grain lo,hi,ref;};
 for(int tree=0;tree<2;++tree){
  std::vector<Item> items;int root=int(tree?d[12].y:d[12].x),end=int(tree?d[13].y:d[13].x),rank=0;
  for(int at=root;at<end;at+=2){if(d[at].w>=0)continue;int refs=int(d[at+1].w),count=int(-d[at].w);
   for(int j=0;j<count;++j){auto ref=d[refs+j];int k=int(ref.x);Grain lo{},hi{};
    if(ref.y==1){lo=hi=d[k];for(int v=1;v<3;++v){lo.x=std::min(lo.x,d[k+v].x);lo.y=std::min(lo.y,d[k+v].y);lo.z=std::min(lo.z,d[k+v].z);hi.x=std::max(hi.x,d[k+v].x);hi.y=std::max(hi.y,d[k+v].y);hi.z=std::max(hi.z,d[k+v].z);}}
    else if(ref.y==4){lo=d[at];hi=d[at+1];} // Conservative fallback for experimental cylinders.
    else {
     // Invert the actual float local-coordinate transform, rather than assume
     // perfectly orthonormal axes. A 1 mm pad covers rounding and r<=30 mm
     // transform error for the validated near-orthonormal basis contract.
     auto a=d[k+1],b=d[k+2],c=d[k+3];double det=a.x*(double(b.y)*c.z-double(b.z)*c.y)-a.y*(double(b.x)*c.z-double(b.z)*c.x)+a.z*(double(b.x)*c.y-double(b.y)*c.x);
     double inv[3][3]={{(double(b.y)*c.z-double(b.z)*c.y)/det,(double(a.z)*c.y-double(a.y)*c.z)/det,(double(a.y)*b.z-double(a.z)*b.y)/det},{(double(b.z)*c.x-double(b.x)*c.z)/det,(double(a.x)*c.z-double(a.z)*c.x)/det,(double(a.z)*b.x-double(a.x)*b.z)/det},{(double(b.x)*c.y-double(b.y)*c.x)/det,(double(a.y)*c.x-double(a.x)*c.y)/det,(double(a.x)*b.y-double(a.y)*b.x)/det}};
     double e[3]{};for(int n=0;n<3;++n)e[n]=std::abs(inv[n][0])*a.w+std::abs(inv[n][1])*b.w+std::abs(inv[n][2])*c.w+.001;
     lo={float(d[k].x-e[0]),float(d[k].y-e[1]),float(d[k].z-e[2]),0};hi={float(d[k].x+e[0]),float(d[k].y+e[1]),float(d[k].z+e[2]),0};
    }
    lo={lo.x-2.e-6f,lo.y-2.e-6f,lo.z-2.e-6f,0};hi={hi.x+2.e-6f,hi.y+2.e-6f,hi.z+2.e-6f,0};int bound=int(d.size());d.push_back(lo);d.push_back(hi);d[refs+j].z=float(bound);items.push_back({lo,hi,{float(at),float(rank),float(1u<<j),float(bound)}});
   }++rank;
  }
  if(mode<6)continue;
  std::vector<int> ids(items.size());std::iota(ids.begin(),ids.end(),0);
  std::vector<std::pair<int,std::vector<int>>> leaves;
  std::function<int(size_t,size_t)> emit=[&](size_t begin,size_t finish){int at=int(d.size());d.resize(d.size()+2);auto lo=items[ids[begin]].lo,hi=items[ids[begin]].hi;
   for(size_t i=begin+1;i<finish;++i){auto a=items[ids[i]].lo,b=items[ids[i]].hi;lo.x=std::min(lo.x,a.x);lo.y=std::min(lo.y,a.y);lo.z=std::min(lo.z,a.z);hi.x=std::max(hi.x,b.x);hi.y=std::max(hi.y,b.y);hi.z=std::max(hi.z,b.z);}
   if(finish-begin<=4){lo.w=-float(finish-begin);leaves.push_back({at,std::vector<int>(ids.begin()+begin,ids.begin()+finish)});}
   else {float extent[]={hi.x-lo.x,hi.y-lo.y,hi.z-lo.z};int axis=int(std::max_element(extent,extent+3)-extent);auto centre=[&](int i){auto a=items[i].lo,b=items[i].hi;return axis==0?a.x+b.x:axis==1?a.y+b.y:a.z+b.z;};size_t mid=(begin+finish)/2;std::nth_element(ids.begin()+begin,ids.begin()+mid,ids.begin()+finish,[&](int a,int b){return centre(a)==centre(b)?a<b:centre(a)<centre(b);});lo.w=float(emit(begin,mid));emit(mid,finish);hi.w=float(d.size());}
   d[at]=lo;d[at+1]=hi;return at;
  };
  int index=emit(0,items.size()),finish=int(d.size());for(auto& leaf:leaves){d[leaf.first+1].w=float(d.size());for(int id:leaf.second)d.push_back(items[id].ref);}
  if(tree){d[9].z=float(index);d[9].w=float(finish);}else {d[9].x=float(index);d[9].y=float(finish);}
 }
 if(d.size()>=16777216)throw std::invalid_argument("candidate index exceeds exact float integer range");
}
}
