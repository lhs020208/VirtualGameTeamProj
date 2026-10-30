#include "powder/plant.hpp"
#include <iostream>
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("data dir");std::string root=argv[1];auto d=powder::read_plant_data(root+"/runtime.plant");auto routes=powder::read_plant_routes(root+"/runtime.routes");auto gates=powder::plant_supplier_gates(d);int hopper=powder::plant_weighing_gate(d);
 powder::plant_set_manual_valves(d,routes,1000,0,0,false,false);
 for(const auto&r:routes)for(int at:r.cells)if(d[at+2].x!=0)throw std::runtime_error("closed valve has flow");
 powder::GranularConfig c;c.plant=true;c.plant_threaded=true;c.kernel_optimize=true;c.plant_data=d;c.count=1;c.radius=.00125f;c.dt=.00001f;c.iterations=8;powder::GranularGPU gpu(c,{{-1.2f,1.4f,3.f,0}},{{0,0,0,0}});
 std::vector<powder::Grain> probes;for(int k:gates)probes.push_back({d[k].x,d[k].y,d[k].z+.0035f,0});probes.push_back({d[hopper].x,d[hopper].y,d[hopper].z+.0035f,0});
 auto verify=[&](unsigned mask,bool h){auto a=gpu.inspect_boundaries(probes,0,false,false);for(size_t i=0;i<a.size();++i){bool open=i<gates.size()?(mask&(1u<<i))!=0:h;float delta=a[i].position.z-probes[i].z;if(open?std::abs(delta)>1e-6f:delta<.0007f)throw std::runtime_error("independent valve collision mismatch");}};
 verify(0,false);
 for(size_t i=0;i<gates.size();++i){unsigned mask=1u<<i;powder::plant_set_manual_valves(d,routes,1000,mask,mask,false,false);gpu.update_plant_field(d);verify(mask,false);size_t j=0;for(const auto&r:routes)if(r.phase==0){if((d[r.cells[0]+2].x>0)!=(j==i))throw std::runtime_error("independent supplier flow mismatch");++j;}}
 d[4].x=1;powder::plant_set_manual_valves(d,routes,1000,1,1,true,true);gpu.update_plant_field(d);verify(1,true);
 for(const auto&r:routes)if((d[r.cells[0]+2].x>0)!=(r.phase==1))throw std::runtime_error("hopper pressure mode mismatch");
 powder::plant_set_manual_valves(d,routes,1000,1,0,true,false);gpu.update_plant_field(d);verify(1,true);for(const auto&r:routes)for(int at:r.cells)if(d[at+2].x!=0)throw std::runtime_error("pending close still has flow");
 if(powder::plant_supplier_closure_clear(d[hopper],{{d[hopper].x,d[hopper].y,d[hopper].z,0}},c.radius))throw std::runtime_error("occupied close allowed");
 if(!powder::plant_supplier_closure_clear(d[hopper],{{0,0,3,0}},c.radius))throw std::runtime_error("empty close rejected");
 d[4].x=0;powder::plant_set_manual_valves(d,routes,1000,0,0,false,false);gpu.update_plant_field(d);verify(0,false);
 int receiver=powder::plant_receiver_gate(d);auto gate=d[receiver];std::vector<powder::Grain> rp{{gate.x,gate.y,gate.z+.0035f,0}};
 for(int phase=0;phase<2;++phase)for(bool open:{false,true,false}){d[4].x=float(phase);powder::plant_set_receiver_valve(d,open);gpu.update_plant_field(d);auto hit=gpu.inspect_boundaries(rp,0,false,false);float dz=hit[0].position.z-rp[0].z;if(open?std::abs(dz)>1e-6f:dz<.0007f)throw std::runtime_error("receiver manual collision mismatch");}
 if(powder::plant_supplier_closure_clear(gate,{{gate.x,gate.y,gate.z,0}},c.radius))throw std::runtime_error("receiver occupied close allowed");
 gpu.advance(16,0,false);double stopped=gpu.angle();gpu.advance(16,15,false);double moving=gpu.angle();gpu.advance(16,0,false);if(moving<=stopped||gpu.angle()!=moving)throw std::runtime_error("mixer start stop angle mismatch");
 std::cout<<"PASS receiver manual gates and mixer start/stop;  all closed; independent supplier gates/flow; hopper open/close; pending-close flow cutoff and occupancy; threaded refresh"<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
