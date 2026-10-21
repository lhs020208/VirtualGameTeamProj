#include "powder/geometry.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
namespace powder {
namespace {
constexpr double eps=1e-10;
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 mul(Vec3 a,double s){return {a.x*s,a.y*s,a.z*s};}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
double norm(Vec3 a){return std::hypot(a.x,a.y,a.z);}
void finite(Vec3 v){if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))throw std::invalid_argument("nonfinite box geometry");}
std::array<double,3> half(const FixedBox& b){return {b.half_m.x,b.half_m.y,b.half_m.z};}
std::array<double,3> local(Vec3 p,const FixedBox& b){auto d=add(p,mul(b.centre_m,-1));return {dot(d,b.axes[0]),dot(d,b.axes[1]),dot(d,b.axes[2])};}
bool inside(Vec3 p,const FixedBox& b){auto x=local(p,b),h=half(b);return std::abs(x[0])<h[0]-eps&&std::abs(x[1])<h[1]-eps&&std::abs(x[2])<h[2]-eps;}
struct Candidate {BoxContact c;std::uint64_t box,group;};
}
void validate_boxes(const std::vector<FixedBox>& boxes){
 std::set<std::uint64_t> ids;
 for(const auto& b:boxes){
  finite(b.centre_m);finite(b.half_m);
  if(b.id>(std::numeric_limits<std::uint64_t>::max()-7)/8||!ids.insert(b.id).second)throw std::invalid_argument("duplicate/oversized box id");
  for(double h:half(b))if(h<=eps)throw std::invalid_argument("box half extent too small");
  for(int i=0;i<3;++i){finite(b.axes[i]);if(std::abs(norm(b.axes[i])-1)>1e-10)throw std::invalid_argument("box axes not unit");for(int j=0;j<i;++j)if(std::abs(dot(b.axes[i],b.axes[j]))>1e-10)throw std::invalid_argument("box axes not orthogonal (shear unsupported)");}
 }
}
std::vector<BoxContact> box_contacts(const Particle& p,const std::vector<FixedBox>& boxes){
 validate(p);validate_boxes(boxes);std::vector<Candidate> candidates;
 for(const auto& b:boxes){
  const auto x=local(p.position_m,b),h=half(b);Vec3 point=b.centre_m;int feature=0,face=0;
  for(int k=0;k<3;++k){point=add(point,mul(b.axes[k],std::clamp(x[k],-h[k],h[k])));if(std::abs(x[k])>h[k]){++feature;face=2*k+(x[k]>0?1:0);}}
  const auto delta=add(p.position_m,mul(point,-1));const double d=norm(delta);
  if(feature==0||d<eps)throw std::invalid_argument("sphere centre on/inside solid box; reduce dt/check initial state");
  const double overlap=p.diameter_m/2-d;if(overlap<=0)continue;
  BoxContact c{b.id*8+static_cast<std::uint64_t>(feature==1?face:6),point,mul(delta,1/d),overlap,feature};
  // Coplanar faces in an explicitly welded assembly share history across seams.
  if(feature==1&&b.weld_group!=0)for(const auto& other:boxes)if(other.weld_group==b.weld_group){
   const auto oh=half(other);
   for(int k=0;k<3;++k)for(int sign:{-1,1}){
    auto n=mul(other.axes[k],sign);auto q=add(other.centre_m,mul(n,oh[k]));
    if(dot(n,c.normal)>1-1e-12&&std::abs(dot(n,add(q,mul(point,-1))))<eps)c.surface_id=std::min(c.surface_id,other.id*8+static_cast<std::uint64_t>(2*k+(sign>0?1:0)));
   }
  }
  candidates.push_back({c,b.id,b.weld_group});
 }
 std::vector<BoxContact> out;
 for(std::size_t i=0;i<candidates.size();++i){const auto& a=candidates[i];bool hidden=false;
  if(a.group!=0){
   for(const auto& b:boxes)if(b.id!=a.box&&b.weld_group==a.group&&inside(a.c.point_m,b))hidden=true;
   // Remove the artificial edge beside a coplanar supporting face, or duplicate faces.
   for(std::size_t j=0;j<candidates.size();++j)if(i!=j){const auto& b=candidates[j];
    if(b.group==a.group&&b.c.feature==1&&dot(a.c.normal,b.c.normal)>0&&std::abs(dot(b.c.normal,add(a.c.point_m,mul(b.c.point_m,-1))))<eps&&b.c.overlap_m>=a.c.overlap_m-eps){
     if(a.c.feature!=1||b.box<a.box)hidden=true;
    }
   }
  }
  if(!hidden)out.push_back(a.c);
 }
 std::sort(out.begin(),out.end(),[](const BoxContact& a,const BoxContact& b){return a.surface_id<b.surface_id;});return out;
}
std::vector<FixedBox> read_boxes(std::istream& in){
 std::string magic;unsigned version;std::size_t count;
 if(!(in>>magic>>version>>count)||magic!="POWDER_BOXES_SI"||version!=1||count>10000)throw std::invalid_argument("invalid geometry header");
 std::vector<FixedBox> out;
 for(std::size_t i=0;i<count;++i){FixedBox b;
  if(!(in>>b.id>>b.weld_group>>b.centre_m.x>>b.centre_m.y>>b.centre_m.z>>b.half_m.x>>b.half_m.y>>b.half_m.z))throw std::invalid_argument("truncated box");
  for(auto& a:b.axes)if(!(in>>a.x>>a.y>>a.z))throw std::invalid_argument("truncated axes");out.push_back(b);
 }
 std::string extra;if(in>>extra)throw std::invalid_argument("trailing geometry data");validate_boxes(out);return out;
}
}
