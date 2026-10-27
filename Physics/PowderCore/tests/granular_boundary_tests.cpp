#include "granular_test_reference.hpp"
#include "../src/granular_mx301_cpu.inc"
int main(){try{
 uint64_t planes=0,parts=0,fullplanes=0,fullparts=0,samples=0;
 std::mt19937 rng(705);std::uniform_real_distribution<float> d(-1.2f,1.2f);
 for(float radius:{.002f,.0063647064f,.02f}){
 GranularConfig c;c.count=1;c.radius=radius;c.test_mixer=true;GranularGPU full(c);c.boundary_candidates=true;GranularGPU culled(c);
 for(float angle:{0.f,.13f,.785398f,1.570796f,3.141593f,5.9f}){
 std::vector<Grain> points;
 for(int i=0;i<12000;++i)points.push_back({d(rng),d(rng),d(rng),float(i%2)});
 for(auto& plane:mxplanes)for(float offset:{-radius,0.f,radius})points.push_back({plane[0]*(plane[3]+offset),plane[1]*(plane[3]+offset),plane[2]*(plane[3]+offset),0});
 for(auto& part:mxparts)for(int corner=0;corner<8;++corner)for(float offset:{-radius,0.f,radius}){
 float x=part[(corner&1)?5:2]+offset,y=part[(corner&2)?6:3]+offset,z=part[(corner&4)?7:4]+offset;
 points.push_back({x,std::cos(angle)*y-std::sin(angle)*z,std::sin(angle)*y+std::cos(angle)*z,0});}
 for(bool shell:{false,true}){
 auto a=full.inspect_boundaries(points,angle,false,shell),b=culled.inspect_boundaries(points,angle,false,shell);
 for(size_t i=0;i<a.size();++i){
 for(auto pair:{std::make_pair(a[i].position,b[i].position),std::make_pair(a[i].normal,b[i].normal),std::make_pair(a[i].velocity,b[i].velocity)}){
 auto v=sub(xyz(pair.first),xyz(pair.second));check(dot(v,v)<1.e-12f,"boundary projection/normal/velocity mismatch");}
 planes+=b[i].plane_tests;parts+=b[i].rotor_tests;fullplanes+=a[i].plane_tests;fullparts+=a[i].rotor_tests;++samples;
 }
 }
 }
 }
 check(planes<fullplanes&&parts<fullparts,"must prune both planes and rotor parts");
 for(bool split:{false,true}){GranularConfig c;c.count=32;c.radius=.006f;c.test_mixer=true;c.candidates=true;c.candidate_reuse=true;c.profile_split=split;c.profile=split;
 std::vector<Grain> p(32),v(32);for(int i=0;i<32;++i)p[i]={-.3f+.02f*i,.25f,-.2f,float(i%2)};
 GranularGPU a(c);a.set_state(p,v);a.advance(128,30,false);c.boundary_candidates=true;GranularGPU b(c);b.set_state(p,v);b.advance(128,30,false);compare(a.snapshot(),b.snapshot(),2.e-6f);
 }
 GranularConfig bad;bad.count=1;bad.boundary_candidates=true;bool rejected=false;try{GranularGPU g(bad);}catch(const std::exception&){rejected=true;}check(rejected,"unsupported geometry must reject flag");
 std::cout<<"PASS boundary samples="<<samples<<" plane visits="<<planes<<"/"<<fullplanes<<" rotor visits="<<parts<<"/"<<fullparts<<"; rotating trajectories fused/split and configuration guard\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}



