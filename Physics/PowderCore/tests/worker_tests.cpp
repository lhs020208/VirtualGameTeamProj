#include "powder/worker.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace powder;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
template<class F>std::shared_ptr<const Snapshot> wait(Worker& w,F predicate){
 const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(15);
 while(std::chrono::steady_clock::now()<end){auto s=w.snapshot();if(predicate(*s))return s;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 throw std::runtime_error("worker acknowledgement timed out");
}
int main(int argc,char** argv){try{
 ParticleSystem w;w.model.law=ContactLaw::Linear;Particle p;p.id=1;p.diameter_m=1;p.material.density_kg_m3=6/3.141592653589793;p.position_m={0,0,.499};w.particles={p};w.planes={{1,{0,0,1},0}};
 Worker worker;auto gen=worker.reset(w,1e-5);wait(worker,[&](auto& s){return s.generation==gen&&s.ready;});
 worker.single_step();auto held=wait(worker,[](auto& s){return s.steps==1;});check(held->histories==1&&!held->running,"single-step history");
 worker.toggle();wait(worker,[](auto& s){return s.running&&s.steps>=8;});worker.pause();auto paused=wait(worker,[](auto& s){return !s.running;});
 std::this_thread::sleep_for(std::chrono::milliseconds(10));check(worker.snapshot()->steps==paused->steps,"pause acknowledgement stable");check(held->steps==1,"immutable snapshot");
 // Retire an active run repeatedly; old publications must never restore it.
 for(int i=0;i<20;++i){worker.toggle();gen=worker.reset(w,1e-5);auto initial=wait(worker,[&](auto& s){return s.generation==gen&&s.ready;});check(initial->steps==0&&initial->histories==0&&!initial->running,"reset isolation");}
 gen=worker.reset(w,1);auto fault=wait(worker,[&](auto& s){return s.generation==gen&&s.ready;});check(!fault->error.empty()&&!fault->running&&fault->steps==0,"fault stops");
 check(argc==2,"fixture argument");std::ifstream f(argv[1]);ParticleSystem many;many.boxes=read_boxes(f);many.model.law=ContactLaw::Linear;many.model.linear={4e6,40,1e6,10,.3};
 for(int i=0;i<300;++i){Particle q;q.id=i+1;q.diameter_m=.04;q.material.density_kg_m3=2500;q.position_m={(i%5-2)*.05,((i/5)%5-2)*.05,4.5+(i/25)*.05};many.particles.push_back(q);}
 gen=worker.reset(many,2e-7);wait(worker,[&](auto& s){return s.generation==gen&&s.ready;});
 auto reference=many;for(int i=0;i<100;++i){worker.single_step();reference.step(2e-7);}
 auto result=wait(worker,[](auto& s){return s.steps==100;});check(result->error.empty()&&result->particles.size()==300,"300 particle steps");
 for(int i=0;i<300;++i){const auto& a=result->particles[i];const auto& b=reference.particles[i];check(a.id==b.id&&a.position_m.x==b.position_m.x&&a.position_m.y==b.position_m.y&&a.position_m.z==b.position_m.z&&a.velocity_m_s.z==b.velocity_m_s.z,"background equals sequential reference, no dropped steps");}
 std::cout<<"300 particles, 53 boxes, 100 fixed steps; compute="<<result->compute_seconds<<"s; mass="<<result->report.total_mass_kg<<"kg\n";
 worker.toggle();wait(worker,[](auto& s){return s.running&&s.steps>100;});
 const auto start=std::chrono::steady_clock::now();for(int i=0;i<1000;++i)(void)worker.snapshot();
 std::cout<<"1000 snapshot reads during computation="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"s\n";
 for(int i=0;i<10;++i){Worker transient;transient.reset(many,2e-7);transient.toggle();} // joins even while initializing/running
 std::cout<<"PASS worker commands, generation isolation, immutable snapshots, 300 particles, shutdown\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
