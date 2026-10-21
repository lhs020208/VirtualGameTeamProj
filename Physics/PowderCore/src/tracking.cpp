#include "powder/tracking.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <set>
namespace powder {
namespace {
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 mul(Vec3 a,double k){return {a.x*k,a.y*k,a.z*k};}
double length(Vec3 a){return std::sqrt(dot(a,a));}
Vec3 average(const std::vector<FixedBox>& boxes){Vec3 c;for(auto& b:boxes)c=add(c,b.centre_m);return mul(c,1.0/boxes.size());}
void cap(Volume& v,Vec3 axis,double lo,double hi){v.sides.push_back({axis,hi});v.sides.push_back({mul(axis,-1),-lo});}
void valid(const TrackingLayout& l){
 if(!l.enabled)return;
 if(!std::isfinite(l.top)||!std::isfinite(l.bottom)||l.top<=l.bottom||(!std::isfinite(length(l.axis))||std::abs(length(l.axis)-1)>1e-8))throw std::invalid_argument("invalid tracking axis/caps");
 for(auto* v:{&l.bore,&l.feed,&l.funnel,&l.collector}){
  if(v->sides.size()<4)throw std::invalid_argument("incomplete tracking volume");
  for(auto b:v->sides)if(!std::isfinite(length(b.normal))||std::abs(length(b.normal)-1)>1e-8||!std::isfinite(b.offset))throw std::invalid_argument("invalid tracking boundary");
 }
}
Volume shell(const std::vector<FixedBox>& boxes,Vec3 axis,bool straight,double& lo,double& hi){
 if(boxes.size()<4)throw std::invalid_argument("tracking requires a closed wall ring");
 validate_boxes(boxes);const Vec3 centre=average(boxes);Volume v;lo=1e100;hi=-1e100;
 std::vector<Vec3> radial;
 for(auto& b:boxes){
  Vec3 n=b.axes[0];if(dot(n,add(b.centre_m,mul(centre,-1)))<0)n=mul(n,-1);
  const double r=dot(n,add(b.centre_m,mul(centre,-1)))-b.half_m.x;
  if(r<=0)throw std::invalid_argument("tracking ring has no inner aperture");
  const double extent=std::abs(dot(axis,b.axes[2]))*b.half_m.z;
  const double a=dot(axis,b.centre_m)-extent,z=dot(axis,b.centre_m)+extent;
  if(straight){
   if(std::abs(dot(n,axis))>1e-8||std::abs(std::abs(dot(axis,b.axes[2]))-1)>1e-8)throw std::invalid_argument("tube walls must share one axis");
   if(!radial.empty()&&(std::abs(a-lo)>1e-7||std::abs(z-hi)>1e-7))throw std::invalid_argument("tube wall end planes differ");
  }
  lo=std::min(lo,a);hi=std::max(hi,z);
  v.sides.push_back({n,dot(n,b.centre_m)-b.half_m.x});
  Vec3 q=add(n,mul(axis,-dot(n,axis)));const double qn=length(q);if(qn<1e-8)throw std::invalid_argument("invalid ring wall direction");radial.push_back(mul(q,1/qn));
 }
 // Sorted planar normals must cover a full circle with no half-circle gap.
 Vec3 x=radial.front(),y{axis.y*x.z-axis.z*x.y,axis.z*x.x-axis.x*x.z,axis.x*x.y-axis.y*x.x};
 std::vector<double> angles;for(auto n:radial)angles.push_back(std::atan2(dot(n,y),dot(n,x)));
 std::sort(angles.begin(),angles.end());constexpr double pi=3.141592653589793;
 for(std::size_t i=0;i<angles.size();++i){double next=i+1<angles.size()?angles[i+1]:angles[0]+2*pi;if(next-angles[i]>=pi-1e-8)throw std::invalid_argument("unbounded tracking ring");}
 return v;
}
}
bool Volume::contains(Vec3 p)const{if(sides.empty())return false;for(auto b:sides)if(dot(b.normal,p)>b.offset)return false;return true;}
TrackingLayout tracking_layout(const std::vector<FixedBox>& feed,const std::vector<FixedBox>& funnel,const std::vector<FixedBox>& tube,const std::vector<FixedBox>& collector){
 TrackingLayout l;if(feed.empty()&&funnel.empty()&&tube.empty()&&collector.empty())return l;
 if(tube.empty()||collector.size()!=5)throw std::invalid_argument("tracking rig requires tube and five collector pieces");
 l.enabled=true;l.axis=tube.front().axes[2];l.bore=shell(tube,l.axis,true,l.bottom,l.top);
 double lo,hi;l.feed=shell(feed,l.axis,false,lo,hi);cap(l.feed,l.axis,lo,hi);
 l.funnel=shell(funnel,l.axis,false,lo,hi);cap(l.funnel,l.axis,lo,hi);
 validate_boxes(collector);
 // Base is the only box whose thinnest dimension is local Z parallel to the tube axis.
 const FixedBox* base=nullptr;
 for(auto& b:collector)if(b.half_m.z<b.half_m.x&&b.half_m.z<b.half_m.y&&std::abs(dot(b.axes[2],l.axis))>1-1e-8){if(base)throw std::invalid_argument("ambiguous collector base");base=&b;}
 if(!base)throw std::invalid_argument("missing collector base");
 lo=dot(l.axis,base->centre_m)+base->half_m.z;hi=1e100;
 for(auto& b:collector)if(&b!=base){
  int k=b.half_m.x<b.half_m.y?0:1;Vec3 n=b.axes[k];double h=k==0?b.half_m.x:b.half_m.y;
  if(dot(n,add(b.centre_m,mul(base->centre_m,-1)))<0)n=mul(n,-1);
  if(std::abs(dot(n,l.axis))>1e-8||std::abs(dot(b.axes[2],l.axis))<1-1e-8)throw std::invalid_argument("collector must align with tube axis");
  l.collector.sides.push_back({n,dot(n,b.centre_m)-h});hi=std::min(hi,dot(l.axis,b.centre_m)+b.half_m.z);
 }
 for(auto a:l.collector.sides){
  if(a.offset<=dot(a.normal,base->centre_m))throw std::invalid_argument("collector interior collapsed");
  int opposite=0,orthogonal=0;for(auto b:l.collector.sides){double d=dot(a.normal,b.normal);if(d< -1+1e-8)++opposite;if(std::abs(d)<1e-8)++orthogonal;}
  if(opposite!=1||orthogonal!=2)throw std::invalid_argument("collector requires four rectangular walls");
 }
 if(hi<=lo)throw std::invalid_argument("invalid collector height");cap(l.collector,l.axis,lo,hi);valid(l);return l;
}
Location Tracker::classify(Vec3 p)const{
 if(!layout_.enabled)return Location::Outside;
 const double z=dot(layout_.axis,p);
 // Shared caps belong to the downstream region; overlaps have explicit priority.
 if(z>layout_.bottom&&z<=layout_.top&&layout_.bore.contains(p))return Location::Tube;
 if(layout_.collector.contains(p))return Location::Collector;
 if(z>layout_.top&&(layout_.feed.contains(p)||layout_.funnel.contains(p)))return Location::Feed;
 return Location::Outside;
}
void Tracker::totals(TrackingSummary& d)const{
 d.counts={};d.mass_kg={};d.total_count=d.records.size();d.total_mass_kg=0;d.unique_entered=d.unique_passed=d.unique_returned=0;d.unique_passed_mass_kg=0;
 for(auto& r:d.records){auto k=static_cast<unsigned>(r.location);++d.counts[k];d.mass_kg[k]+=r.mass_kg;d.total_mass_kg+=r.mass_kg;d.unique_entered+=r.ever_entered;d.unique_passed+=r.ever_passed;d.unique_returned+=r.ever_returned;if(r.ever_passed)d.unique_passed_mass_kg+=r.mass_kg;}
 double sum=0;for(double m:d.mass_kg)sum+=m;d.mass_error_kg=sum-initial_mass_;
}
void Tracker::reset(TrackingLayout layout,const std::vector<Particle>& particles){
 valid(layout);validate_particles(particles);layout_=std::move(layout);data_={};data_.enabled=layout_.enabled;time_=0;initial_mass_=0;index_.clear();
 for(auto& p:particles){ParticleTrack r;r.id=p.id;r.previous=p.position_m;r.location=classify(p.position_m);r.mass_kg=sphere_properties(p.diameter_m,p.material).mass_kg;initial_mass_+=r.mass_kg;index_[p.id]=data_.records.size();data_.records.push_back(r);}totals(data_);
}
void Tracker::admit(const std::vector<Particle>& particles,double time){
 if(!std::isfinite(time)||time!=time_)throw std::invalid_argument("birth time must match tracking clock");
 validate_particles(particles);auto next=*this;
 for(auto& p:particles){
  if(next.index_.count(p.id))throw std::invalid_argument("duplicate admitted particle ID");
  ParticleTrack r;r.id=p.id;r.previous=p.position_m;r.location=classify(p.position_m);r.location_since_s=time;r.birth_time_s=time;r.mass_kg=sphere_properties(p.diameter_m,p.material).mass_kg;
  next.initial_mass_+=r.mass_kg;next.index_[p.id]=next.data_.records.size();next.data_.records.push_back(r);
 }next.totals(next.data_);*this=std::move(next);
}
void Tracker::advance(const std::vector<Particle>& particles,double time){
 if(!std::isfinite(time)||time<=time_)throw std::invalid_argument("tracking time must increase");
 if(particles.size()!=index_.size())throw std::invalid_argument("tracking particle count changed");
 validate_particles(particles);auto next=data_;std::set<ParticleId> seen;
 for(auto& p:particles){
  auto it=index_.find(p.id);if(it==index_.end()||!seen.insert(p.id).second)throw std::invalid_argument("tracking particle ID changed");
  auto& r=next.records[it->second];const double mass=sphere_properties(p.diameter_m,p.material).mass_kg;
  if(mass!=r.mass_kg)throw std::invalid_argument("tracking particle mass changed");
  const Vec3 a=r.previous,b=p.position_m,delta=add(b,mul(a,-1));
  struct Hit {double alpha;int kind;};std::vector<Hit> hits;
  if(layout_.enabled){
   const double za=dot(layout_.axis,a),zb=dot(layout_.axis,b);
   auto gate=[&](double z,int down,int up){
    // Exact endpoint ownership avoids duplicate events when a step lands on a plane.
    const bool d=za>z&&zb<=z,u=za<=z&&zb>z;if(!d&&!u)return;
    double alpha=(z-za)/(zb-za);if(layout_.bore.contains(add(a,mul(delta,alpha))))hits.push_back({alpha,d?down:up});
   };
   gate(layout_.top,0,3);gate(layout_.bottom,1,2);
   for(auto plane:layout_.bore.sides){double da=dot(plane.normal,a)-plane.offset,db=dot(plane.normal,b)-plane.offset;if(da<=0&&db>0){double alpha=-da/(db-da);double z=za+alpha*(zb-za);if(z>=layout_.bottom&&z<=layout_.top)hits.push_back({alpha,4});}}
   std::sort(hits.begin(),hits.end(),[](auto x,auto y){return x.alpha<y.alpha||(x.alpha==y.alpha&&x.kind>y.kind);});
   for(auto h:hits){
    if(h.kind==4){r.admitted=false;continue;}
    double t=time_+h.alpha*(time-time_);bool passed=false;++next.crossings[h.kind];
    if(h.kind==0){r.admitted=true;r.ever_entered=true;r.last_entry_s=t;if(r.first_entry_s<0)r.first_entry_s=t;}
    if(h.kind==1){r.last_exit_s=t;if(r.first_exit_s<0)r.first_exit_s=t;if(r.admitted){passed=true;r.ever_passed=true;++next.passages;}r.admitted=false;}
    if(h.kind==3){if(r.admitted){r.ever_returned=true;++next.returns;}r.admitted=false;}
    r.events.push_back({static_cast<Crossing>(h.kind),t,passed});
   }
   if(!layout_.bore.contains(b))r.admitted=false;
  }
  const auto location=classify(b);if(location!=r.location){r.location=location;r.location_since_s=time;}r.previous=b;
 }
 totals(next);data_=std::move(next);time_=time;
}
}
