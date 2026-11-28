#pragma once
#include "powder/granular.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace powder {
using MixerPoint=std::array<double,3>;
struct MixerObstacleBox {
 MixerPoint centre{},half{};
 std::array<MixerPoint,3> axes{{{1,0,0},{0,1,0},{0,0,1}}};
};
namespace mixer_obstacle {
using Triangle=std::array<MixerPoint,3>;
inline MixerPoint sub(MixerPoint a,MixerPoint b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
inline double dot(MixerPoint a,MixerPoint b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline MixerPoint cross(MixerPoint a,MixerPoint b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
inline MixerPoint point(Grain p){return {p.x,p.y,p.z};}
inline void validate(const MixerObstacleBox& b){
 for(int i=0;i<3;++i){
  if(!std::isfinite(b.centre[i])||!std::isfinite(b.half[i])||b.half[i]<=0)
   throw std::invalid_argument("invalid fixed mixer obstacle box");
  for(double x:b.axes[i])if(!std::isfinite(x))throw std::invalid_argument("invalid obstacle axis");
  if(std::abs(dot(b.axes[i],b.axes[i])-1)>1.e-6)throw std::invalid_argument("obstacle axes must be unit length");
  for(int j=0;j<i;++j)if(std::abs(dot(b.axes[i],b.axes[j]))>1.e-6)throw std::invalid_argument("obstacle axes must be orthogonal");
 }
}
// Exact triangle/OBB separating-axis test, in box-local coordinates. Padding is
// a geometric sweep bound, not a larger collision proxy around the whole rotor.
inline bool intersects(const Triangle& t,const MixerObstacleBox& b,MixerPoint pivot,double angle,double padding=0){
 Triangle v;const double co=std::cos(angle),si=std::sin(angle);
 for(int j=0;j<3;++j){auto p=sub(t[j],pivot);
  p={p[0]+pivot[0],co*p[1]-si*p[2]+pivot[1],si*p[1]+co*p[2]+pivot[2]};p=sub(p,b.centre);
  for(int k=0;k<3;++k)v[j][k]=dot(p,b.axes[k]);
 }
 MixerPoint h{b.half[0]+padding,b.half[1]+padding,b.half[2]+padding};
 auto separated=[&](MixerPoint axis){
  double p0=dot(v[0],axis),p1=dot(v[1],axis),p2=dot(v[2],axis);
  double r=h[0]*std::abs(axis[0])+h[1]*std::abs(axis[1])+h[2]*std::abs(axis[2]);
  return std::min({p0,p1,p2})>r||std::max({p0,p1,p2})<-r;
 };
 const std::array<MixerPoint,3> basis{{{1,0,0},{0,1,0},{0,0,1}}};
 for(auto axis:basis)if(separated(axis))return false;
 for(int j=0;j<3;++j){auto edge=sub(v[(j+1)%3],v[j]);for(auto axis:basis)if(separated(cross(edge,axis)))return false;}
 return !separated(cross(sub(v[1],v[0]),sub(v[2],v[0])));
}
inline std::vector<Triangle> rotor_triangles(const std::vector<Grain>& data){
 std::vector<Triangle> triangles;
 if(data.size()<16)throw std::invalid_argument("missing mixer geometry");
 int root=int(data[0].y);if(root<0)return triangles;
 std::vector<int> stack{root};size_t visited=0;
 while(!stack.empty()){
  int k=stack.back();stack.pop_back();
  if(k<16||size_t(k+1)>=data.size()||++visited>data.size())throw std::invalid_argument("invalid rotor tree");
  const auto a=data[k],b=data[k+1];
  if(a.w>=0){stack.push_back(int(a.w));stack.push_back(int(b.w));}
  else for(int j=0;j<int(-a.w);++j){int at=int(b.w)+3*j;
   if(at<16||size_t(at+2)>=data.size())throw std::invalid_argument("invalid rotor triangle");
   triangles.push_back({point(data[at]),point(data[at+1]),point(data[at+2])});
  }
 }
 return triangles;
}
struct Candidate { Triangle triangle; MixerObstacleBox box; double radius; };
// Bound the entire continuous angular interval by padding the box at its
// midpoint by R*halfWidth. Every triangle vertex moves at most that far; convex
// combinations do too. Thus a clear interval cannot hide a thin-wall crossing.
inline PlantMixerStops build(const std::vector<Triangle>& triangles,MixerPoint pivot,
                            const std::vector<MixerObstacleBox>& boxes){
 PlantMixerStops out;if(boxes.empty())return out;
 constexpr double tau=6.283185307179586,step=tau/4096,margin=.00001;
 std::vector<Candidate> candidates;
 for(const auto& b:boxes){validate(b);
  double ex=0;for(int i=0;i<3;++i)ex+=std::abs(b.axes[i][0])*b.half[i];
  // A box's bounding sphere is used only to reject impossible radial contact.
  double radial=std::hypot(b.centre[1]-pivot[1],b.centre[2]-pivot[2]);
  double minRadius=std::max(0.,radial-std::sqrt(dot(b.half,b.half)));
  for(const auto& t:triangles){
   if(std::min({t[0][0],t[1][0],t[2][0]})>b.centre[0]+ex+margin||
      std::max({t[0][0],t[1][0],t[2][0]})<b.centre[0]-ex-margin)continue;
   double radius=0;for(auto p:t)radius=std::max(radius,std::hypot(p[1]-pivot[1],p[2]-pivot[2]));
   if(radius+margin<minRadius)continue;
   if(intersects(t,b,pivot,0,margin))throw std::invalid_argument("Mixer obstacle overlaps the initial rotor; move the block and restart");
   candidates.push_back({t,b,radius});
  }
 }
 if(candidates.empty())return out;
 auto clear=[&](double from,double to){
  for(const auto& c:candidates)if(intersects(c.triangle,c.box,pivot,(from+to)/2,c.radius*std::abs(to-from)/2+margin))return false;
  return true;
 };
 auto limit=[&](int sign){
  double safe=0;
  for(int i=1;i<=4096;++i){double next=sign*i*step;
   if(clear(safe,next)){safe=next;continue;}
   // Refine only the first uncertain interval, staying on its proven clear side.
   for(int j=0;j<20;++j){double mid=(safe+next)/2;if(clear(safe,mid))safe=mid;else next=mid;}
   return safe;
  }return sign*tau;
 };
 double hi=limit(1);if(hi==tau)return out; // Full-turn sweep is clear in both directions.
 double lo=limit(-1);out.enabled=true;
 // Round inwards so the float OpenCL limit cannot go beyond the double proof.
 out.lower_rad=std::nextafter(float(lo),0.f);out.upper_rad=std::nextafter(float(hi),0.f);out.validate();return out;
}
}
inline PlantMixerStops build_mixer_obstacle_stops(const std::vector<Grain>& data,const std::vector<MixerObstacleBox>& boxes){
 if(boxes.empty())return {};
 return mixer_obstacle::build(mixer_obstacle::rotor_triangles(data),mixer_obstacle::point(data.at(1)),boxes);
}
}
