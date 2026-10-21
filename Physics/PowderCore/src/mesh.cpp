#include "powder/mesh.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
namespace powder {namespace {
constexpr double tol=1e-10;
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 mul(Vec3 a,double s){return {a.x*s,a.y*s,a.z*s};}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double norm(Vec3 a){return std::hypot(a.x,a.y,a.z);}
double axis(Vec3 a,int k){return k==0?a.x:k==1?a.y:a.z;}
void require(bool b,const char* message){if(!b)throw std::invalid_argument(message);}
bool finite(Vec3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
MeshEdge edge(std::size_t a,std::size_t b){return {std::min(a,b),std::max(a,b)};}
struct Bounds {Vec3 lo{1e100,1e100,1e100},hi{-1e100,-1e100,-1e100};
 void include(Vec3 p){lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};}
 bool meets(const Bounds& b)const{return lo.x<=b.hi.x&&hi.x>=b.lo.x&&lo.y<=b.hi.y&&hi.y>=b.lo.y&&lo.z<=b.hi.z&&hi.z>=b.lo.z;}
 bool contains(Vec3 p)const{return p.x>=lo.x&&p.x<=hi.x&&p.y>=lo.y&&p.y<=hi.y&&p.z>=lo.z&&p.z<=hi.z;}
};
struct Closest {Vec3 point;int kind=0;std::size_t a=0,b=0;}; // face 0, edge 1, vertex 2
Closest closest(Vec3 p,const std::array<std::size_t,3>& t,const std::vector<Vec3>& v){
 auto a=v[t[0]],b=v[t[1]],c=v[t[2]],ab=sub(b,a),ac=sub(c,a),ap=sub(p,a);double d1=dot(ab,ap),d2=dot(ac,ap);
 if(d1<=0&&d2<=0)return {a,2,t[0],0};auto bp=sub(p,b);double d3=dot(ab,bp),d4=dot(ac,bp);if(d3>=0&&d4<=d3)return {b,2,t[1],0};
 double vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return {add(a,mul(ab,d1/(d1-d3))),1,t[0],t[1]};auto cp=sub(p,c);double d5=dot(ab,cp),d6=dot(ac,cp);if(d6>=0&&d5<=d6)return {c,2,t[2],0};
 double vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return {add(a,mul(ac,d2/(d2-d6))),1,t[0],t[2]};double va=d3*d6-d5*d4;if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0)return {add(b,mul(sub(c,b),(d4-d3)/((d4-d3)+(d5-d6)))),1,t[1],t[2]};
 double inv=1/(va+vb+vc);return {add(a,add(mul(ab,vb*inv),mul(ac,vc*inv))),0,0,0};
}
bool segment_hit(Vec3 a,Vec3 b,Vec3 p,Vec3 q,Vec3 r,double& time){auto n=cross(sub(q,p),sub(r,p));double l=norm(n);n=mul(n,1/l);double da=dot(sub(a,p),n),db=dot(sub(b,p),n);if((da>tol&&db>tol)||(da<-tol&&db<-tol)||std::abs(da-db)<1e-15)return false;time=da/(da-db);if(time<-tol||time>1+tol)return false;Vec3 x=add(a,mul(sub(b,a),time));auto e0=sub(q,p),e1=sub(r,p),d=sub(x,p);double aa=dot(e0,e0),bb=dot(e0,e1),cc=dot(e1,e1),dd=dot(d,e0),ee=dot(d,e1),det=aa*cc-bb*bb;double u=(cc*dd-bb*ee)/det,v=(aa*ee-bb*dd)/det;return u>=-tol&&v>=-tol&&u+v<=1+tol;}
}
struct StaticTriangleMesh::Data {
 bool axis_box=false;
 MeshInput input;MeshDiagnostics diag;std::vector<Vec3> normals;std::vector<std::size_t> patch,order;
 std::map<MeshEdge,std::vector<std::size_t>> edges;std::map<MeshEdge,std::uint64_t> edge_ids;
 struct Node {Bounds box;std::size_t begin=0,end=0;int left=-1,right=-1;};std::vector<Node> tree;
 int build(std::size_t begin,std::size_t end){Node n;n.begin=begin;n.end=end;for(auto i=begin;i<end;++i)for(auto v:input.triangles[order[i]])n.box.include(input.vertices[v]);int index=static_cast<int>(tree.size());tree.push_back(n);if(end-begin>8){auto d=sub(n.box.hi,n.box.lo);int k=d.x>=d.y&&d.x>=d.z?0:d.y>=d.z?1:2;std::stable_sort(order.begin()+begin,order.begin()+end,[&](auto a,auto b){double x=0,y=0;for(int j=0;j<3;++j){x+=axis(input.vertices[input.triangles[a][j]],k);y+=axis(input.vertices[input.triangles[b][j]],k);}return x==y?a<b:x<y;});auto mid=begin+(end-begin)/2;int left=build(begin,mid),right=build(mid,end);tree[index].left=left;tree[index].right=right;}return index;}
 void candidates(int index,const Bounds& b,std::vector<std::size_t>& out)const{const auto& n=tree[index];if(!n.box.meets(b))return;if(n.left<0){for(auto i=n.begin;i<n.end;++i)out.push_back(order[i]);}else{candidates(n.left,b,out);candidates(n.right,b,out);}}
 std::vector<std::size_t> query_box(const Bounds& b)const{std::vector<std::size_t> out;candidates(0,b,out);std::sort(out.begin(),out.end());return out;}
 // Solid angle avoids parity ambiguity when a ray exactly touches an edge or vertex.
 bool inside(Vec3 p)const{if(!input.closed_solid||!tree[0].box.contains(p))return false;double angle=0;for(auto t:input.triangles){auto a=sub(input.vertices[t[0]],p),b=sub(input.vertices[t[1]],p),c=sub(input.vertices[t[2]],p);double la=norm(a),lb=norm(b),lc=norm(c);require(std::min({la,lb,lc})>tol,"sphere centre on mesh vertex");angle+=2*std::atan2(dot(a,cross(b,c)),la*lb*lc+dot(a,b)*lc+dot(b,c)*la+dot(c,a)*lb);}return std::abs(angle)>2*3.14159265358979323846;}
};
StaticTriangleMesh::StaticTriangleMesh(const MeshInput& source){auto d=std::make_shared<Data>();d->input=source;auto& m=d->input;auto& diag=d->diag;
 require(m.vertices.size()>=3&&m.vertices.size()<=200000&&m.triangles.size()>0&&m.triangles.size()<=200000,"mesh size outside 3..200000 vertices / 1..200000 triangles");require(std::isfinite(m.metres_per_unit)&&m.metres_per_unit>0,"mesh units must explicitly be positive metres per unit");require(std::isfinite(m.minimum_diagonal_m)&&std::isfinite(m.maximum_diagonal_m)&&m.minimum_diagonal_m>tol&&m.maximum_diagonal_m>=m.minimum_diagonal_m,"mesh expected SI size contract missing");
 Bounds bounds;std::map<std::array<double,3>,std::size_t> positions;for(std::size_t i=0;i<m.vertices.size();++i){auto& v=m.vertices[i];v=mul(v,m.metres_per_unit);require(finite(v)&&norm(v)<1e6,"nonfinite/out of range SI mesh coordinate");require(positions.emplace(std::array<double,3>{v.x,v.y,v.z},i).second,"duplicate mesh vertices: weld exact render seams before analysis");bounds.include(v);}m.metres_per_unit=1;diag.diagonal_m=norm(sub(bounds.hi,bounds.lo));require(diag.diagonal_m>=m.minimum_diagonal_m&&diag.diagonal_m<=m.maximum_diagonal_m,"mesh SI size inconsistent with expected dimensions; check cm/mm/m units");diag.minimum_edge_m=diag.diagonal_m;
 std::set<std::array<std::size_t,3>> unique;d->patch.resize(m.triangles.size());std::iota(d->patch.begin(),d->patch.end(),0);std::vector<bool> used(m.vertices.size(),false);
 for(std::size_t i=0;i<m.triangles.size();++i){auto t=m.triangles[i];for(auto v:t){require(v<m.vertices.size(),"mesh triangle index out of range");used[v]=true;}auto sorted=t;std::sort(sorted.begin(),sorted.end());require(unique.insert(sorted).second,"duplicate mesh face (including reversed face)");auto a=m.vertices[t[0]],b=m.vertices[t[1]],c=m.vertices[t[2]],n=cross(sub(b,a),sub(c,a));double area=norm(n);double longest=std::max({norm(sub(a,b)),norm(sub(b,c)),norm(sub(c,a))});require(longest>tol&&area>longest*longest*1e-10,"degenerate/sliver mesh triangle");d->normals.push_back(mul(n,1/area));for(int k=0;k<3;++k){auto e=edge(t[k],t[(k+1)%3]);d->edges[e].push_back(i);diag.minimum_edge_m=std::min(diag.minimum_edge_m,norm(sub(m.vertices[e[0]],m.vertices[e[1]])));}}
 require(std::all_of(used.begin(),used.end(),[](bool x){return x;}),"unused mesh vertices");
 std::map<MeshEdge,BoundaryKind> labels;for(auto b:m.boundaries){require(b.kind==BoundaryKind::Inlet||b.kind==BoundaryKind::Outlet||b.kind==BoundaryKind::Perimeter,"invalid mesh boundary kind");require(b.edge[0]!=b.edge[1]&&b.edge[0]<m.vertices.size()&&b.edge[1]<m.vertices.size(),"invalid boundary vertex");require(labels.emplace(edge(b.edge[0],b.edge[1]),b.kind).second,"duplicate mesh boundary label");}
 auto root=[&](std::size_t i){while(d->patch[i]!=i)i=d->patch[i];return i;};std::uint64_t eid=0;
 for(const auto& entry:d->edges){const auto& e=entry.first;const auto& ts=entry.second;require(ts.size()<=2,"nonmanifold mesh edge");d->edge_ids[e]=eid++;if(ts.size()==1){require(!m.closed_solid,"closed mesh has a hole");auto l=labels.find(e);require(l!=labels.end(),"unclassified open edge: possible unintended gap");if(l->second==BoundaryKind::Inlet)++diag.inlet_edges;else if(l->second==BoundaryKind::Outlet)++diag.outlet_edges;else ++diag.perimeter_edges;labels.erase(l);}else{auto direction=[&](std::size_t i){auto t=m.triangles[i];for(int k=0;k<3;++k)if(t[k]==e[0]&&t[(k+1)%3]==e[1])return 1;return -1;};require(direction(ts[0])!=direction(ts[1]),"inconsistent triangle winding at shared edge");if(dot(d->normals[ts[0]],d->normals[ts[1]])>1-1e-12){auto a=root(ts[0]),b=root(ts[1]);d->patch[std::max(a,b)]=std::min(a,b);}}}
 require(labels.empty(),"boundary label does not name an open edge");for(std::size_t i=0;i<d->patch.size();++i)d->patch[i]=root(i);diag.patches=std::set<std::size_t>(d->patch.begin(),d->patch.end()).size();
 if(m.closed_solid){std::vector<bool> visited(m.triangles.size(),false);std::vector<std::size_t> stack{0};visited[0]=true;while(!stack.empty()){auto i=stack.back();stack.pop_back();auto t=m.triangles[i];for(int k=0;k<3;++k)for(auto j:d->edges.at(edge(t[k],t[(k+1)%3])))if(!visited[j]){visited[j]=true;stack.push_back(j);}}require(std::all_of(visited.begin(),visited.end(),[](bool v){return v;}),"closed mesh must contain one connected shell; split separate solids");double volume=0;for(auto t:m.triangles)volume+=dot(sub(m.vertices[t[0]],bounds.lo),cross(sub(m.vertices[t[1]],bounds.lo),sub(m.vertices[t[2]],bounds.lo)))/6;require(volume>tol*tol*tol,"closed mesh must have outward winding and positive volume");}
 diag.vertices=m.vertices.size();diag.triangles=m.triangles.size();diag.two_sided_sheet=!m.closed_solid;d->order.resize(m.triangles.size());std::iota(d->order.begin(),d->order.end(),0);d->build(0,d->order.size());
 // Exact recognition only: no fitting or removal of small gaps/features.
 const std::vector<std::array<std::size_t,3>> box_triangles={{{0,2,3}},{{0,3,1}},{{4,5,7}},{{4,7,6}},{{0,1,5}},{{0,5,4}},{{2,6,7}},{{2,7,3}},{{0,4,6}},{{0,6,2}},{{1,3,7}},{{1,7,5}}};
 d->axis_box=m.closed_solid&&m.vertices.size()==8&&m.triangles==box_triangles;
 if(d->axis_box)for(std::size_t i=0;i<8;++i){const auto& v=m.vertices[i];if(v.x!=((i&1)?bounds.hi.x:bounds.lo.x)||v.y!=((i&2)?bounds.hi.y:bounds.lo.y)||v.z!=((i&4)?bounds.hi.z:bounds.lo.z))d->axis_box=false;}
 data_=std::move(d);
}
const MeshDiagnostics& StaticTriangleMesh::diagnostics()const{return data_->diag;}
MeshQuery StaticTriangleMesh::query(const Particle& p,bool brute)const{return query_impl(p,brute,false);}
MeshQuery StaticTriangleMesh::query_optimized(const Particle& p)const{return query_impl(p,false,true);}
double StaticTriangleMesh::radius_bound(Vec3 pivot)const{require(finite(pivot),"invalid radius pivot");double result=0;for(auto p:data_->input.vertices)result=std::max(result,norm(sub(p,pivot)));return result;}
MeshQuery StaticTriangleMesh::query_impl(const Particle& p,bool brute,bool optimized)const{validate(p);const auto& d=*data_;double radius=p.diameter_m/2;Bounds b;b.include(sub(p.position_m,{radius,radius,radius}));b.include(add(p.position_m,{radius,radius,radius}));
 if(optimized){
  if(!d.tree[0].box.meets(b)){MeshQuery result;result.bounds_rejects=1;return result;}
  if(d.axis_box){const auto& bounds=d.tree[0].box;int outside=0,which=0;double distance=0,sign=0;bool interior=true;
   for(int k=0;k<3;++k){double x=axis(p.position_m,k),lo=axis(bounds.lo,k),hi=axis(bounds.hi,k);
    if(x<lo||x>hi){++outside;which=k;sign=x>hi?1.:-1.;distance=x>hi?x-hi:lo-x;}
    else if(x-lo<radius+tol||hi-x<radius+tol)interior=false;
   }
   if(outside==1&&interior&&distance>tol){MeshQuery result;result.analytic_queries=1;
    if(distance<radius){auto point=p.position_m;Vec3 normal;const auto& side=sign>0?bounds.hi:bounds.lo;
     if(which==0){point.x=side.x;normal.x=sign;}else if(which==1){point.y=side.y;normal.y=sign;}else{point.z=side.z;normal.z=sign;}
     const std::uint64_t face=which==0?(sign>0?10:8):which==1?(sign>0?6:4):(sign>0?2:0);
     result.contacts.push_back({face,point,normal,radius-distance});}return result;
   }
  }
 }
 require(!d.inside(p.position_m),"sphere centre inside closed mesh solid");std::vector<std::size_t> candidates;if(brute){candidates.resize(d.input.triangles.size());std::iota(candidates.begin(),candidates.end(),0);}else candidates=d.query_box(b);
 struct Candidate{MeshHit hit;std::size_t patch;bool face;};std::vector<Candidate> raw;std::set<std::size_t> face_patches;MeshQuery result;
 for(auto i:candidates){++result.triangle_checks;auto t=d.input.triangles[i];auto q=closest(p.position_m,t,d.input.vertices);auto delta=sub(p.position_m,q.point);double distance=norm(delta);if(distance>=radius)continue;require(distance>tol,"sphere centre on mesh surface; reduce dt/check initial state");if(d.input.closed_solid&&dot(delta,d.normals[i])<-tol)continue;
  // A projection onto a shared edge/vertex is a face contact when it lies on the plane normal.
  bool face=norm(cross(delta,d.normals[i]))<=tol;std::uint64_t feature=static_cast<std::uint64_t>(d.patch[i]);
  if(face){face_patches.insert(d.patch[i]);}else if(q.kind==1){auto e=edge(q.a,q.b);const auto& adjacent=d.edges.at(e);if(adjacent.size()==2&&d.patch[adjacent[0]]==d.patch[adjacent[1]])continue;feature=d.input.triangles.size()+d.edge_ids.at(e);}else if(q.kind==2){bool boundary_vertex=false;for(const auto& entry:d.edges)if(entry.first[0]==q.a||entry.first[1]==q.a){const auto& adjacent=entry.second;if(adjacent.size()==1||d.patch[adjacent[0]]!=d.patch[adjacent[1]]){boundary_vertex=true;break;}}if(!boundary_vertex)continue;feature=d.input.triangles.size()+d.edges.size()+q.a;}else continue;
  raw.push_back({{feature,q.point,mul(delta,1/distance),radius-distance},d.patch[i],face});
 }
 for(const auto& c:raw){if(!c.face&&face_patches.count(c.patch))continue;bool duplicate=false;for(auto& out:result.contacts)if(out.feature_id==c.hit.feature_id||norm(sub(out.point_m,c.hit.point_m))<tol){if(c.hit.feature_id<out.feature_id)out=c.hit;duplicate=true;break;}if(!duplicate)result.contacts.push_back(c.hit);}
 std::sort(result.contacts.begin(),result.contacts.end(),[](const auto& a,const auto& b){return a.feature_id<b.feature_id;});return result;
}
void StaticTriangleMesh::guard_motion(Vec3 from,Vec3 to)const{require(finite(from)&&finite(to),"nonfinite mesh motion");if(norm(sub(to,from))<1e-16)return;Bounds b;b.include(from);b.include(to);b.lo=sub(b.lo,{tol,tol,tol});b.hi=add(b.hi,{tol,tol,tol});for(auto i:data_->query_box(b)){auto t=data_->input.triangles[i];double time=0;if(segment_hit(from,to,data_->input.vertices[t[0]],data_->input.vertices[t[1]],data_->input.vertices[t[2]],time))throw std::invalid_argument("particle centre crossed triangle sheet; reduce dt/check contact stiffness");}}
std::string MeshDiagnostics::text()const{std::ostringstream o;o<<std::setprecision(17)<<"triangles "<<triangles<<"\nvertices "<<vertices<<"\ncoplanar_patches "<<patches<<"\ndiagonal_m "<<diagonal_m<<"\nminimum_edge_m "<<minimum_edge_m<<"\ninlet_edges "<<inlet_edges<<"\noutlet_edges "<<outlet_edges<<"\nperimeter_edges "<<perimeter_edges<<"\ntwo_sided_zero_thickness_sheet "<<two_sided_sheet<<"\n";return o.str();}
std::string StaticTriangleMesh::serialize()const{const auto& m=data_->input;std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17)<<"POWDER_TRIANGLES 1 1 "<<m.minimum_diagonal_m<<' '<<m.maximum_diagonal_m<<' '<<m.closed_solid<<' '<<m.vertices.size()<<' '<<m.triangles.size()<<' '<<m.boundaries.size()<<'\n';for(auto v:m.vertices)o<<v.x<<' '<<v.y<<' '<<v.z<<'\n';for(auto t:m.triangles)o<<t[0]<<' '<<t[1]<<' '<<t[2]<<'\n';for(auto b:m.boundaries)o<<b.edge[0]<<' '<<b.edge[1]<<' '<<(b.kind==BoundaryKind::Inlet?"inlet":b.kind==BoundaryKind::Outlet?"outlet":"perimeter")<<'\n';return o.str();}
MeshInput read_triangle_mesh(std::istream& in){MeshInput m;std::string magic;int version=0,solid=0;std::size_t nv=0,nt=0,nb=0;require(bool(in>>magic>>version>>m.metres_per_unit>>m.minimum_diagonal_m>>m.maximum_diagonal_m>>solid>>nv>>nt>>nb)&&magic=="POWDER_TRIANGLES"&&version==1&&(solid==0||solid==1)&&nv<=200000&&nt<=200000&&nb<=600000,"invalid triangle mesh header");m.closed_solid=solid!=0;m.vertices.resize(nv);m.triangles.resize(nt);for(auto& v:m.vertices)require(bool(in>>v.x>>v.y>>v.z),"truncated mesh vertex");for(auto& t:m.triangles)require(bool(in>>t[0]>>t[1]>>t[2]),"truncated mesh triangle");for(std::size_t i=0;i<nb;++i){MeshBoundary b;std::string kind;require(bool(in>>b.edge[0]>>b.edge[1]>>kind),"truncated mesh boundary");require(kind=="inlet"||kind=="outlet"||kind=="perimeter","unknown mesh boundary kind");b.kind=kind=="inlet"?BoundaryKind::Inlet:kind=="outlet"?BoundaryKind::Outlet:BoundaryKind::Perimeter;m.boundaries.push_back(b);}std::string extra;require(!(in>>extra),"trailing mesh data");return m;}
MeshInput triangulated_box(const FixedBox& b){validate_boxes({b});MeshInput m;m.closed_solid=true;for(int z:{-1,1})for(int y:{-1,1})for(int x:{-1,1})m.vertices.push_back(add(b.centre_m,add(mul(b.axes[0],b.half_m.x*x),add(mul(b.axes[1],b.half_m.y*y),mul(b.axes[2],b.half_m.z*z)))));m.triangles={{{0,2,3}},{{0,3,1}},{{4,5,7}},{{4,7,6}},{{0,1,5}},{{0,5,4}},{{2,6,7}},{{2,7,3}},{{0,4,6}},{{0,6,2}},{{1,3,7}},{{1,7,5}}};if(dot(cross(b.axes[0],b.axes[1]),b.axes[2])<0)for(auto& t:m.triangles)std::swap(t[1],t[2]);Bounds bounds;for(auto v:m.vertices)bounds.include(v);double diagonal=norm(sub(bounds.hi,bounds.lo));m.minimum_diagonal_m=diagonal*.999;m.maximum_diagonal_m=diagonal*1.001;return m;}
}
