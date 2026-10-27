#include "granular_test_reference.hpp"
std::uint64_t audited=0;
void audit(GranularGPU& gpu,GranularConfig c,const std::vector<Grain>& p,bool expectOverflow){
 gpu.set_state(p,std::vector<Grain>(p.size()));auto list=gpu.inspect_candidates();check((list.overflow_particles>0)==expectOverflow,"unexpected overflow behavior");
 for(unsigned i=0;i<p.size();++i){std::set<unsigned> ids;for(auto j:list.neighbors[i]){check(j<p.size()&&j!=i,"invalid candidate ID");check(ids.insert(j).second,"duplicate candidate");}
  for(unsigned j=0;j<p.size();++j)if(j!=i){double dx=double(p[i].x)-p[j].x,dy=double(p[i].y)-p[j].y,dz=double(p[i].z)-p[j].z;if(dx*dx+dy*dy+dz*dz<4*double(c.radius)*c.radius){check(ids.count(j)==1,"CPU all-pairs contact missing");++audited;}}
 }
}
int main(){try{
 GranularConfig c;c.count=128;c.radius=.004f;c.candidates=true;c.iterations=16;std::mt19937 rng(70203);std::uniform_real_distribution<float> d(-.025f,.025f);std::vector<Grain> points(c.count),vel(c.count);
 for(unsigned i=0;i<c.count;++i){points[i]={.30f+d(rng),.18f+d(rng),.35f+d(rng),float(i%2)};vel[i]={d(rng),d(rng),d(rng),0};}
 for(bool mx:{false,true})for(bool fast:{false,true}){c.test_mixer=mx;c.fast_bounds=fast;GranularGPU gpu(c);for(int sample=0;sample<4;++sample){for(auto& p:points){p.x=.30f+d(rng);p.y=.18f+d(rng);p.z=mx?-.20f+d(rng):.35f+d(rng);}audit(gpu,c,points,false);}}
 c.test_mixer=false;c.fast_bounds=true;for(auto& p:points)p.z=.35f+d(rng);
 auto expected=reference(c,points,vel);GranularGPU list(c);list.set_state(points,vel);list.advance(1,0,false);compare(list.snapshot(),expected,2.e-6f);
 c.candidates=false;GranularGPU legacy(c);legacy.set_state(points,vel);legacy.advance(1,0,false);compare(legacy.snapshot(),expected,2.e-6f);compare(list.snapshot(),legacy.snapshot(),2.e-6f);
 c.candidates=true;c.count=70;std::vector<Grain> dense(70,{.3f,.18f,.35f,0});GranularGPU crowded(c);audit(crowded,c,dense,true);
 c.count=3;c.candidate_capacity=1;c.profile=true;points={{.295f,.18f,.35f,0},{.3f,.18f,.35f,1},{.305f,.18f,.35f,0}};vel.assign(3,{});
 GranularGPU fallback(c);audit(fallback,c,points,true);fallback.set_state(points,vel);fallback.advance(1,0,false);auto timing=fallback.timing();check(timing.candidate_fallback_particle_iterations>0,"overflow must use full traversal");check(timing.candidate_builds==c.iterations&&timing.calls[8]==c.iterations,"candidate build/event count");
 c.candidates=false;GranularGPU fallbackReference(c);fallbackReference.set_state(points,vel);fallbackReference.advance(1,0,false);compare(fallback.snapshot(),fallbackReference.snapshot(),2.e-6f);
 // The first correction creates a NEW contact (middle/right) for the next iteration.
 c.candidates=true;c.count=3;c.radius=.01f;c.candidate_capacity=64;
 points={{.28f,.18f,.35f,0},{.295f,.18f,.35f,1},{.316f,.18f,.35f,0}};vel.assign(3,{});
 GranularGPU createdContact(c);createdContact.set_state(points,vel);auto before=createdContact.inspect_candidates();check(std::find(before.neighbors[1].begin(),before.neighbors[1].end(),2)==before.neighbors[1].end(),"fixture must start without middle/right contact");
 createdContact.advance(1,0,false);compare(createdContact.snapshot(),reference(c,points,vel),2.e-6f);
 // Boundary/cell edges, touching particles, then reset into an entirely different list.
 c.candidates=true;c.candidate_capacity=64;c.count=4;c.radius=.01f;GranularGPU edge(c);
 points={{0,.18f,.35f,0},{.02f,.18f,.35f,1},{-.019999f,.18f,.35f,0},{-.04f,.18f,.35f,1}};audit(edge,c,points,false);
 points={{.449f,.249f,.39f,0},{.431f,.249f,.39f,1},{-.449f,-.249f,.39f,0},{-.431f,-.249f,.39f,1}};audit(edge,c,points,false);
 check(edge.time()==0,"inspection must not advance physics");edge.set_state(points,std::vector<Grain>(4));check(edge.timing().candidate_builds==0,"reset must clear diagnostics");
 for(bool split:{false,true}){c.profile_split=split;c.count=2;points={{.43f,.2f,.011f,0},{.43f,.219f,.011f,1}};vel.assign(2,{});c.candidates=false;GranularGPU a(c);c.candidates=true;GranularGPU b(c);a.set_state(points,vel);b.set_state(points,vel);a.advance(128,30,false);b.advance(128,30,false);compare(a.snapshot(),b.snapshot(),2.e-6f);}
 bool bad=false;c.candidate_capacity=0;try{GranularGPU invalid(c);}catch(const std::exception&){bad=true;}check(bad,"zero capacity rejected");bad=false;c.candidate_capacity=257;try{GranularGPU invalid(c);}catch(const std::exception&){bad=true;}check(bad,"excessive capacity rejected");
 std::cout<<"PASS candidates: "<<audited<<" directed CPU all-pairs contacts, random scenes, cell edges, dense overflow, full-traversal fallback, CPU Jacobi reference, wall trajectory, split profile, reset and capacity guards\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
