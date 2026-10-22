#include "powder/granular.hpp"
#include <iostream>
#include <cmath>
#include <stdexcept>
void check(bool x,const char* s){if(!x)throw std::runtime_error(s);}
int main(){try{
 powder::GranularConfig c;c.count=1;c.radius=.01f;c.dt=.00025f;c.iterations=16;powder::GranularGPU g(c);
 g.set_state({{.3f,.2f,.35f,1}},{{0,0,0,0}});g.advance(200,0,false);auto p=g.snapshot();double t=g.time();std::cout<<"freefall z="<<p[0].z<<" expected="<<(.35-9.81*t*(t+c.dt)/2)<<std::endl;check(std::abs(p[0].z-(.35-9.81*t*(t+c.dt)/2))<2.e-4,"free fall trajectory");check(p[0].w==1,"group identity");
 g.set_state({{0,0,.05f,0}},{{0,0,0,0}});g.advance(1000,0,false);p=g.snapshot();check(p[0].z>=.0099f&&p[0].z<.0102f,"closed gate leakage/rest");g.advance(1000,0,true);g.advance(1000,0,true);p=g.snapshot();check(p[0].z<-.25f&&p[0].z>=-.5901f,"open gate descent and receiver floor");
 g.set_state({{.051f,0,.03f,0}},{{-.02f,0,0,0}});g.advance(1000,0,true);g.advance(1000,0,true);p=g.snapshot();check(std::isfinite(p[0].z)&&p[0].z>=-.5901f,"outlet lip stability");
 g.set_state({{.3f,.2f,.35f,0}},{{100,0,0,0}});bool rejected=false;try{g.advance(1,0,false);}catch(const std::exception&){rejected=true;}check(rejected,"unsafe displacement guard");rejected=false;try{g.advance(1,0,false);}catch(const std::exception&){rejected=true;}check(rejected,"rejected batch must require reset");g.set_state({{.3f,.2f,.35f,0}},{{0,0,0,0}});g.advance(1,0,false);check(std::isfinite(g.snapshot()[0].z),"reset after rejected batch");
 c.count=2;powder::GranularGPU pair(c);pair.set_state({{-.009f,.2f,.35f,0},{.009f,.2f,.35f,1}},{{0,0,0,0},{0,0,0,0}});pair.advance(1,0,false);auto q=pair.snapshot();check(q[1].x-q[0].x>.0199f,"pair overlap resolution");check(std::abs(q[1].x+q[0].x)<1.e-6,"pair symmetry");check(q[0].w==0&&q[1].w==1,"persistent group ids");

 // Dispatch batching must not change the integrated single-particle trajectory.
 c.count=1;c.test_mixer=false;powder::GranularGPU batchA(c),batchB(c);
 batchA.set_state({{.3f,.2f,.35f,1}},{{0,0,0,0}});batchB.set_state({{.3f,.2f,.35f,1}},{{0,0,0,0}});
 batchA.advance(200,0,false);for(int k=0;k<25;k++)batchB.advance(8,0,false);
 check(std::abs(batchA.snapshot()[0].z-batchB.snapshot()[0].z)<1.e-7,"batch trajectory equivalence");
 c.test_mixer=true;powder::GranularGPU asset(c);
 asset.set_state({{.72f,0,-.28f,1}},{{1,0,0,0}});asset.advance(1000,0,false);auto ap=asset.snapshot()[0];
 check(ap.x<=.7401f&&ap.x>=-.7401f&&ap.z>=-.3151f,"test asset end wall and floor");check(ap.w==1,"test asset group identity");
 bool bad=false;c.friction=2;try{powder::GranularGPU invalid(c);}catch(const std::exception&){bad=true;}check(bad,"invalid friction rejected");
 std::cout<<"PASS free fall, pair symmetry, identity, closed/open gate, receiver, lip, guard\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

