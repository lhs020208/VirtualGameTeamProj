#include "granular_test_reference.hpp"
#include <limits>
void run_compare(GranularConfig c,const std::vector<Grain>& p,const std::vector<Grain>& v,unsigned steps,bool rotation){
 c.kernel_optimize=false;c.candidates=false;c.candidate_reuse=false;c.candidate_audit=false;GranularGPU plain(c);plain.set_state(p,v);plain.advance(steps,rotation?30:0,false);
 c.kernel_optimize=true;c.candidates=true;c.candidate_reuse=true;c.candidate_audit=true;GranularGPU cache(c);cache.set_state(p,v);cache.advance(steps,rotation?30:0,false);compare(cache.snapshot(),plain.snapshot(),2.e-6f);
 auto t=cache.timing();check(t.reuse_checks==steps*c.iterations,"must check displacement before every solve");check(t.candidate_reuses+t.candidate_builds==t.reuse_checks,"build/reuse accounting");
}
int main(){try{
 GranularConfig c;c.kernel_optimize=true;c.count=2;c.radius=.01f;c.dt=.00025f;c.candidates=true;c.candidate_reuse=true;c.candidate_audit=true;c.profile=true;
 // Pair starts OUTSIDE the cached skin and approaches while gravity also moves it.
 std::vector<Grain> p{{.28f,.18f,.35f,0},{.307f,.18f,.35f,1}},v{{.4f,0,0,0},{-.4f,0,0,0}};
 GranularGPU g(c);g.set_state(p,v);auto list=g.inspect_candidates();check(list.neighbors[0].empty(),"closing pair must start outside skin");g.set_state(p,v);g.advance(128,0,false);auto t=g.timing();check(t.argument_cache_hits>t.argument_calls,"argument cache should skip unchanged arguments");check(t.candidate_reuses>0&&t.candidate_builds>1&&t.candidate_builds<t.reuse_checks,"cache must reuse and rebuild");check(t.calls[11]==128*c.iterations&&t.calls[10]==t.calls[11],"all-pairs oracle must inspect every solve");run_compare(c,p,v,128,false);
 auto whole=g.snapshot();GranularGPU batches(c);batches.set_state(p,v);for(int k=0;k<4;++k)batches.advance(32,0,false);compare(whole,batches.snapshot(),2.e-6f);check(batches.timing().candidate_builds==t.candidate_builds,"batch boundary must retain valid cache");
 g.set_state(p,v);check(g.timing().reuse_checks==0&&g.timing().candidate_builds==0,"reset counters");g.advance(1,0,false);check(g.timing().candidate_builds>=1,"reset invalidates cache");
 // Contact correction alone creates a new pair beyond the tiny original skin.
 c.count=3;c.candidate_skin_ratio=.01f;p={{.28f,.18f,.35f,0},{.295f,.18f,.35f,1},{.316f,.18f,.35f,0}};v.assign(3,{});
 GranularGPU correction(c);correction.set_state(p,v);correction.advance(1,0,false);compare(correction.snapshot(),reference(c,p,v),2.e-6f);check(correction.timing().candidate_builds>1,"position correction must force rebuild");run_compare(c,p,v,1,false);
 // Overflow forces a current grid, not fallback against a stale cached grid.
 c.candidate_skin_ratio=.5f;c.candidate_capacity=1;p={{.295f,.18f,.35f,0},{.3f,.18f,.35f,1},{.305f,.18f,.35f,0}};
 GranularGPU overflow(c);overflow.set_state(p,v);overflow.advance(1,0,false);check(overflow.timing().candidate_fallback_particle_iterations>0,"exercise forced overflow fallback");compare(overflow.snapshot(),reference(c,p,v),2.e-6f);run_compare(c,p,v,1,false);
 // Synthetic shaft penetration forces a large boundary correction, then a rebuild.
 c.candidate_capacity=64;c.count=1;v.assign(1,{});
 for(bool mx:{false,true}){c.test_mixer=mx;p={{0,.02f,mx?0.f:.21f,1}};GranularGPU rotor(c);rotor.set_state(p,v);rotor.advance(1,30,false);check(rotor.timing().candidate_builds>1,"rotor projection must invalidate reuse");run_compare(c,p,v,1,true);}
 c.test_mixer=false;c.count=2;p={{.43f,.2f,.011f,0},{.43f,.219f,.011f,1}};v.assign(2,{});
 for(bool split:{false,true}){c.profile_split=split;run_compare(c,p,v,128,true);}c.profile_split=false;
 // Independent CPU all-pairs Jacobi comparison across skin sizes and grid modes.
 c.count=32;c.radius=.004f;std::mt19937 rng(70204);std::uniform_real_distribution<float> d(-.015f,.015f);p.resize(32);v.resize(32);
 for(float skin:{.01f,.5f,2.f})for(bool fast:{false,true}){c.candidate_skin_ratio=skin;c.fast_bounds=fast;for(unsigned i=0;i<32;++i){p[i]={.3f+d(rng),.18f+d(rng),.35f+d(rng),float(i%2)};v[i]={d(rng),d(rng),d(rng),0};}GranularGPU random(c);random.set_state(p,v);random.advance(1,0,false);compare(random.snapshot(),reference(c,p,v),2.e-6f);}
 c.count=1;c.candidate_skin_ratio=.5f;GranularGPU guarded(c);guarded.set_state({{5,0,0,0}},{{0,0,0,0}});bool rejected=false;try{guarded.advance(1,0,false);}catch(const std::exception&){rejected=true;}check(rejected,"reuse must retain domain guard");guarded.set_state({{.3f,.18f,.35f,0}},{{0,0,0,0}});guarded.advance(1,0,false);
 for(float skin:{0.f,3.f,std::numeric_limits<float>::quiet_NaN()}){c.candidate_skin_ratio=skin;rejected=false;try{GranularGPU invalid(c);}catch(const std::exception&){rejected=true;}check(rejected,"invalid skin rejected");}
 c.candidate_skin_ratio=.5f;c.candidates=false;rejected=false;try{GranularGPU invalid(c);}catch(const std::exception&){rejected=true;}check(rejected,"reuse without candidates rejected");
 c.candidates=true;c.count=4097;rejected=false;try{GranularGPU invalid(c);}catch(const std::exception&){rejected=true;}check(rejected,"large all-pairs audit rejected");
 // Changing dynamic arguments, odd ping-pong iteration count, and uncached reset reference.
 c={};c.count=2;c.radius=.004f;c.iterations=3;c.candidates=true;c.candidate_reuse=true;
 p={{.3f,.18f,.35f,0},{.31f,.18f,.35f,1}};v.assign(2,{});GranularGPU baseline(c);baseline.set_state(p,v);c.kernel_optimize=true;GranularGPU optimized(c);optimized.set_state(p,v);
 for(float rpm:{0.f,30.f,-10.f,0.f}){bool gate=rpm!=0;baseline.advance(7,rpm,gate);optimized.advance(7,rpm,gate);compare(optimized.snapshot(),baseline.snapshot(),2.e-6f);}
 c.test_mixer=true;GranularGPU diagnostic(c);for(int i=0;i<3;++i){auto result=diagnostic.inspect_boundaries({{0,.3f,-.2f,0}},float(i),false,i%2);check(result.size()==1,"temporary diagnostic buffer rebinding");}
 // Diagnostic launch sizes retain the same contact law; timing span decomposes
 // into event execution plus within-collection gaps on the in-order queue.
 c={};c.count=2;c.radius=.004f;c.profile=true;
 p={{.3f,.18f,.35f,0},{.307f,.18f,.35f,1}};v.assign(2,{});
 for(unsigned local:{64u,128u,256u}){c.diagnostic_local_size=local;GranularGPU measured(c);measured.set_state(p,v);measured.advance(1,0,false);compare(measured.snapshot(),reference(c,p,v),2.e-6f);auto mt=measured.timing();double busy=0;for(auto seconds:mt.gpu_seconds)busy+=seconds;check(mt.gpu_batch_span>0&&std::abs(mt.gpu_batch_span-busy-mt.gpu_intercommand_gap)<1.e-6,"GPU timing decomposition");check(mt.advance_thread_cpu>=0&&mt.submit_thread_cpu<=mt.advance_thread_cpu,"thread CPU accounting");}
 c.diagnostic_local_size=17;bool badLocal=false;try{GranularGPU bad(c);}catch(const std::exception&){badLocal=true;}check(badLocal,"invalid local size rejected");
 std::cout<<"PASS optimized layout/argument cache: approaching unseen pair, correction-created contact, shaft/rotor projection, overflow fallback, batch persistence, reset, skin/grid variants, CPU reference, every-iteration all-pairs oracle, domain and configuration guards\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
