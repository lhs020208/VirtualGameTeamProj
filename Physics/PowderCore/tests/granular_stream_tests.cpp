#include "granular_test_reference.hpp"
#include <chrono>
#include <thread>
int main(){try{
 for(bool profile:{false,true}){GranularConfig c;c.count=32;c.radius=.004f;c.profile=profile;c.candidates=true;c.candidate_reuse=true;
 std::vector<Grain> p(32),v(32),received;for(int i=0;i<32;++i)p[i]={.2f+.009f*(i%8),.1f+.009f*(i/8),.3f,float(i%2)};
 GranularGPU g(c);g.set_state(p,v);g.request_snapshot();bool rejected=false;try{g.request_snapshot();}catch(const std::exception&){rejected=true;}check(rejected,"double enqueue rejected");
 g.advance(8,0,false);check(g.receive_snapshot(received,true),"read completes");compare(received,p,0.0000001f);check(!g.receive_snapshot(received),"consume once");
 for(int i=0;i<20;++i){auto expected=g.snapshot();g.request_snapshot();g.advance(4,0,false);check(g.receive_snapshot(received,true),"queued read completes");compare(received,expected,0.0000001f);}
 GranularGPU reference(c);reference.set_state(p,v);reference.advance(88,0,false);compare(g.snapshot(),reference.snapshot(),2.e-6f);
 auto state=g.snapshot();g.request_snapshot();auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!g.receive_snapshot(received)){check(std::chrono::steady_clock::now()<deadline,"poll completion timeout");std::this_thread::yield();}compare(received,state,1.e-7f);
g.request_snapshot();g.set_state(p,v);check(!g.receive_snapshot(received),"reset discards pending generation");compare(g.snapshot(),p,0.0000001f);
 g.request_snapshot(); // destruction must finish DMA before freeing host storage
 }
 std::cout<<"PASS async snapshots: request-time identity after advance, pending guard, repeated buffer reuse, consume once, reset and destruction, profiling on/off\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
