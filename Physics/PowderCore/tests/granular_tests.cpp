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
 // External initialization bypasses legacy packing and retains supplied velocity/group.
 c.count=1;c.test_mixer=false;c.radius=.01f;
 powder::GranularGPU supplied(c,{{.3f,.2f,.35f,1}},{{.1f,0,0,0}});
 check(supplied.time()==0&&supplied.snapshot()[0].w==1,"external initial snapshot");supplied.advance(200,0,false);auto ep=supplied.snapshot()[0];
 check(std::abs(ep.x-(.3+.1*supplied.time()))<2.e-5,"external initial velocity");
 bool invalid=false;try{powder::validate_granular_initial(c,{{.449f,.2f,.35f,0}});}catch(const std::exception&){invalid=true;}check(invalid,"external wall penetration rejected");
 c.count=2;invalid=false;try{powder::validate_granular_initial(c,{{.3f,.2f,.35f,0},{.3f,.2f,.35f,1}});}catch(const std::exception&){invalid=true;}check(invalid,"cross-emitter overlap rejected");
 c.count=1;c.test_mixer=true;invalid=false;try{powder::validate_granular_initial(c,{{0,0,0,0}});}catch(const std::exception&){invalid=true;}check(invalid,"external rotor penetration rejected");
 // Profiling must preserve contact, wall friction and rotating-boundary trajectories.
 for(bool mx:{false,true}){
  powder::GranularConfig cfg;cfg.count=2;cfg.radius=.01f;cfg.test_mixer=mx;
  std::vector<powder::Grain> state=mx?std::vector<powder::Grain>{{.72f,0,-.31f,0},{.72f,.019f,-.31f,1}}:std::vector<powder::Grain>{{.43f,.2f,.011f,0},{.43f,.219f,.011f,1}};
  std::vector<powder::Grain> vel(2,{.05f,.02f,0,0});
  powder::GranularGPU plain(cfg);plain.set_state(state,vel);plain.advance(128,30,false);auto expected=plain.snapshot();
  for(bool split:{false,true}){
   cfg.profile=true;cfg.profile_split=split;powder::GranularGPU measured(cfg);measured.set_state(state,vel);measured.advance(128,30,false);auto actual=measured.snapshot();auto timing=measured.timing();
   for(unsigned i=0;i<2;++i){check(std::abs(actual[i].x-expected[i].x)<2.e-6&&std::abs(actual[i].y-expected[i].y)<2.e-6&&std::abs(actual[i].z-expected[i].z)<2.e-6,"profiling contact/wall trajectory changed");check(actual[i].w==expected[i].w,"profiling group changed");}
   check(timing.calls[0]==128*cfg.iterations&&timing.calls[1]==timing.calls[0]&&timing.calls[2]==128&&timing.calls[3]==timing.calls[0]&&timing.calls[4]==128&&timing.calls[7]==1,"profiling dispatch counts");
   check(timing.calls[5]==(split?timing.calls[0]:0)&&timing.calls[6]==(split?128:0),"split dispatch counts");
   check(timing.gpu_seconds[3]>0&&timing.gpu_seconds[7]>0&&timing.advance_wall>=timing.submit_wall+timing.guard_wait_wall,"profiling clock accounting");
   measured.set_state(state,vel);auto reset=measured.timing();check(reset.advance_wall==0&&reset.calls[0]==0&&reset.profiling_enabled,"reset clears profiling counters");
  }
 }
 // Deliberately intersect the shaft to exercise moving-boundary projection/probe.
 // One step avoids treating this synthetic penetrating input as a valid trajectory.
 for(bool mx:{false,true}){
  powder::GranularConfig cfg;cfg.count=1;cfg.radius=.01f;cfg.test_mixer=mx;
  std::vector<powder::Grain> state{{0,.02f,mx?0.f:.21f,1}},vel(1);
  powder::GranularGPU plain(cfg);plain.set_state(state,vel);plain.advance(1,30,false);auto expected=plain.snapshot()[0];
  check(std::abs(expected.y-state[0].y)+std::abs(expected.z-state[0].z)>.001,"rotor fixture must project");
  cfg.profile=true;cfg.profile_split=true;powder::GranularGPU split(cfg);split.set_state(state,vel);split.advance(1,30,false);auto actual=split.snapshot()[0];
  check(std::abs(actual.x-expected.x)<2.e-6&&std::abs(actual.y-expected.y)<2.e-6&&std::abs(actual.z-expected.z)<2.e-6,"split rotor projection changed");
 }
 powder::GranularConfig invalidProfile;invalidProfile.profile_split=true;bool profileRejected=false;try{powder::GranularGPU invalid(invalidProfile);}catch(const std::exception&){profileRejected=true;}check(profileRejected,"split requires profiling");
 bool bad=false;c.friction=2;try{powder::GranularGPU invalid(c);}catch(const std::exception&){bad=true;}check(bad,"invalid friction rejected");
 std::cout<<"PASS free fall, pair symmetry, identity, closed/open gate, receiver, lip, guard\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

