#include "powder/clump_worker.hpp"
#include <iostream>
#include <iomanip>
#include <atomic>
using namespace powder;
using Clock=std::chrono::steady_clock;
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"missing rejection");}
ClumpRunState fixture(std::uint64_t steps=12000){
 ClumpRunState s;s.World=clump_demo();MixingConfig c;c.duration_s=steps*c.dt;c.sample_s=100*c.dt;c.rpm=5;
 s.World.bodies=mixing_charge(c);s.Mixing.emplace(c,s.World.bodies);s.World.boxes.clear();
 FixedBox floor;floor.id=1;floor.centre_m={0,0,-.05};floor.half_m={.5,.4,.05};s.World.meshes.emplace_back(triangulated_box(floor));
 FixedBox paddle;paddle.id=2;paddle.centre_m={0,0,.04};paddle.half_m={.18,.01,.03};PrescribedMotion motion;motion.pivot_m={0,0,.04};motion.rpm=5;
 s.World.moving_meshes.emplace_back(StaticTriangleMesh(triangulated_box(paddle)),motion);return s;
}
void wait_finished(ClumpWorker& w,int sleep_ms){
 auto start=Clock::now();auto previous=w.snapshot();
 for(;;){auto s=w.snapshot();check(s->generation==previous->generation&&s->steps>=previous->steps&&s->time_s>=previous->time_s,"time reversal");check(s->error.empty(),"worker error");previous=s;
  if(s->finished)break;check(std::chrono::duration<double>(Clock::now()-start).count()<20,"worker timeout");
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
 }
}
int main(){try{
 std::cout<<std::setprecision(17);
 auto initial=fixture();auto reference=initial;const auto start=Clock::now();
 for(std::uint64_t n=1;n<=12000;++n){reference.Report=reference.World.step(reference.Dt);reference.Steps=n;reference.Mixing->record(reference.World.bodies,n);}
 const double direct=std::chrono::duration<double>(Clock::now()-start).count();
 const auto expected=serialize_clump_state(reference.World);auto files=reference.Mixing->files(reference.World.bodies,reference.Steps);
 std::cout<<"direct_wall_s="<<direct<<'\n';
 for(int repeat=0;repeat<3;++repeat)for(int hz:{1,30,120}){
  ClumpWorkerOptions o;o.paced=false;o.snapshot_hz=hz;o.profile=(repeat==2);ClumpWorker w(initial,o);auto held=w.snapshot();auto held_position=held->bodies[0].position_m.z;
  const auto begin=Clock::now();w.run();wait_finished(w,hz==1?50:hz==30?16:1);w.pause();auto s=w.snapshot();auto cp=w.checkpoint();
  const double wall=std::chrono::duration<double>(Clock::now()-begin).count();
  check(serialize_clump_state(cp.World)==expected,"FPS/snapshot/profile altered body trajectory or wall work");
  check(cp.Mixing->files(cp.World.bodies,cp.Steps)==files,"FPS altered mixing measurements");
  check(cp.World.histories()==reference.World.histories(),"history count mismatch");
  check(s->steps==12000&&s->finished&&!s->running&&s->sample_count==121,"final state/sample count");
  check(s->publications<=static_cast<std::uint64_t>(std::ceil(wall*hz))+5,"snapshot cadence exceeded");
  check(held->steps==0&&held->bodies[0].position_m.z==held_position,"published state mutated");
  // History continuity: exporting/copying and resuming keeps the next physical step identical.
  cp.World.step(cp.Dt);auto next=reference.World;next.step(cp.Dt);check(serialize_clump_state(cp.World)==serialize_clump_state(next),"checkpoint lost contact history");
  std::cout<<"repeat="<<repeat<<" hz="<<hz<<" wall_s="<<wall<<" compute_s="<<s->compute_s<<" snapshot_s="<<s->snapshot_s<<" publications="<<s->publications<<" rate="<<s->time_s/wall<<'\n';
 }
 ClumpWorkerOptions o;o.paced=false;ClumpWorker w(initial,o);
 w.single_step();check(w.snapshot()->steps==1&&!w.snapshot()->running,"single step barrier");auto expected_one=initial.World;expected_one.step(initial.Dt);check(serialize_clump_state(w.checkpoint().World)==serialize_clump_state(expected_one),"single step parity");
 w.run();std::this_thread::sleep_for(std::chrono::milliseconds(4));w.pause();auto p=w.snapshot();std::this_thread::sleep_for(std::chrono::milliseconds(5));check(w.snapshot()==p,"pause still advancing");w.single_step();check(w.snapshot()->steps==p->steps+1,"paused N not exactly one step");
 w.run();rejects([&]{w.checkpoint();});w.pause();
 auto bad=initial;bad.Dt=1;auto gen=w.snapshot()->generation;rejects([&]{w.reset(bad,o);});check(w.snapshot()->generation==gen,"invalid reset replaced generation");
 // Concurrent commands serialize; no stale snapshots are republished after reset returns.
 std::thread controls([&]{for(int i=0;i<20;++i){w.run();w.pause();w.single_step();}});
 for(int i=0;i<20;++i){w.reset(initial,o);auto s=w.snapshot();check(s->generation>gen,"reset generation");gen=s->generation;}
 controls.join();w.pause();w.reset(initial,o);check(w.snapshot()->steps==0&&w.snapshot()->time_s==0,"reset clean state");
 std::this_thread::sleep_for(std::chrono::milliseconds(10));check(w.snapshot()->steps==0,"retired step published");
 // Independent pacing and no-tick operation. Paused time is excluded on resume.
 o.paced=true;o.target_rate=.001;w.reset(initial,o);auto before=Clock::now();w.run();std::this_thread::sleep_for(std::chrono::milliseconds(100));w.pause();auto paced=w.snapshot();
 check(paced->steps>0&&paced->time_s<=std::chrono::duration<double>(Clock::now()-before).count()*.001+initial.Dt,"pacing target or no-tick execution");
 std::this_thread::sleep_for(std::chrono::milliseconds(100));w.run();w.pause();check(w.snapshot()->steps<=paced->steps+2,"paused time catch-up");
 auto stop_start=Clock::now();for(int i=0;i<20;++i){ClumpWorker temp(initial);temp.run();}check(std::chrono::duration<double>(Clock::now()-stop_start).count()<5,"shutdown joins stalled");
 rejects([&]{ClumpWorker invalid(bad);});o.snapshot_hz=0;rejects([&]{w.set_options(o);});
 // A valid first step followed by a safety rejection must stop and preserve last accepted state.
 auto failure=initial;failure.Mixing.reset();failure.World.moving_meshes.clear();failure.World.gravity_m_s2.z=-1e8;
 auto serial=failure;std::string error;for(int i=0;i<100;++i){try{serial.World.step(serial.Dt);++serial.Steps;}catch(const std::exception& e){error=e.what();break;}}
 check(!error.empty()&&serial.Steps>0,"failure fixture needs accepted steps before rejection");
 o.snapshot_hz=30;o.paced=false;w.reset(failure,o);w.run();auto deadline=Clock::now()+std::chrono::seconds(3);
 while(w.snapshot()->error.empty()&&Clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 auto failed=w.snapshot();check(!failed->running&&failed->error==error&&failed->steps==serial.Steps,"failure status publication");
 check(serialize_clump_state(w.checkpoint().World)==serialize_clump_state(serial.World),"failed step committed partial state");
 w.single_step();check(w.snapshot()->steps==serial.Steps,"step after error advanced");w.reset(initial,o);check(w.snapshot()->error.empty()&&w.snapshot()->steps==0,"reset clears failure");
 std::cout<<"PASS fixed-step exact state/measurement/history parity, polling cadence, bounded mailbox, immutable snapshots, commands, reset generation, pacing, shutdown\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
