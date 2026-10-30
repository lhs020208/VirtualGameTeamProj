#include "powder/plant.hpp"
#include <iostream>
#include <cstring>
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("data directory");std::string root=argv[1];auto d=powder::read_plant_data(root+"/runtime.plant"),t=powder::plant_threaded_upload(d);std::vector<int> triangles;
 for(int tree=0;tree<2;++tree){int rootNode=int(tree?d[0].y:d[0].x);std::vector<int> expected,actual,stack{rootNode};while(!stack.empty()){int at=stack.back();stack.pop_back();auto lo=d[at],hi=d[at+1];if(lo.w>=0){stack.push_back(int(lo.w));stack.push_back(int(hi.w));}else for(int i=0;i<int(-lo.w);++i)expected.push_back(int(hi.w)+3*i);}
  int at=int(tree?t[8].z:t[8].x),end=int(tree?t[8].w:t[8].y);while(at<end){auto lo=t[at],hi=t[at+1];if(lo.w>=0){if(hi.w<=at||hi.w>end)throw std::runtime_error("invalid escape");at=int(lo.w);}else{for(int i=0;i<int(-lo.w);++i)actual.push_back(int(hi.w)+3*i);at+=2;}}
  if(actual!=expected)throw std::runtime_error("projection order changed");triangles.insert(triangles.end(),expected.begin(),expected.end());
 }
 powder::GranularConfig c;c.plant=true;c.kernel_optimize=true;c.plant_data=d;c.count=1;c.radius=.00125f;c.dt=.00001f;c.iterations=8;std::vector<powder::Grain> initial{{-1.2f,1.4f,3.f,0}},v(1);powder::GranularGPU base(c,initial,v);c.plant_threaded=true;powder::GranularGPU opt(c,initial,v);
 unsigned compared=0;for(int phase=0;phase<3;++phase){d[4].x=float(phase);powder::solve_plant_pressure(d,powder::read_plant_routes(root+"/runtime.routes"),phase?1000:0);base.update_plant_field(d);opt.update_plant_field(d);
  for(float angle:{0.f,.3f,1.1f}){std::vector<powder::Grain> probes{{3,0,2.1635f,0},{3.1395f,0,2.16f,0},{0,0,.6035f,0}};
   for(size_t j=0;j<triangles.size();j+=83){int at=triangles[j];auto a=d[at],b=d[at+1],z=d[at+2];float nx=(b.y-a.y)*(z.z-a.z)-(b.z-a.z)*(z.y-a.y),ny=(b.z-a.z)*(z.x-a.x)-(b.x-a.x)*(z.z-a.z),nz=(b.x-a.x)*(z.y-a.y)-(b.y-a.y)*(z.x-a.x);float len=std::hypot(nx,ny,nz);float offset=j%2?.0005f:-.0005f;probes.push_back({(a.x+b.x+z.x)/3+offset*nx/len,(a.y+b.y+z.y)/3+offset*ny/len,(a.z+b.z+z.z)/3+offset*nz/len,0});}
   auto a=base.inspect_boundaries(probes,angle,false,false),b=opt.inspect_boundaries(probes,angle,false,false);for(size_t i=0;i<a.size();++i){for(auto pair:{std::make_pair(a[i].position,b[i].position),std::make_pair(a[i].normal,b[i].normal),std::make_pair(a[i].velocity,b[i].velocity)})if(std::memcmp(&pair.first,&pair.second,sizeof(powder::Grain)))throw std::runtime_error("boundary regression mismatch");}compared+=unsigned(a.size());
  }
 }
 std::cout<<"PASS exact triangle order; "<<compared<<" boundary samples at 3 phases x 3 angles; field refresh preserved threaded nodes; extra GPU bytes="<<(t.size()-d.size())*sizeof(powder::Grain)<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
