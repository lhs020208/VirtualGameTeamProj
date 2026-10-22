#pragma once
#include "powder/clump.hpp"
#include <set>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <memory>
namespace powder {
enum class ProcessStage { Supply, Storage, Transfer, Receiver, Mixing };
struct ProcessRegion {ProcessStage stage=ProcessStage::Supply;ClumpWorld world;};
struct Residence {std::size_t owner=0;double entered_s=0;std::array<double,5> seconds{};};
struct HandoffReceipt {std::uint64_t sequence=0;std::size_t from=0,to=0;double time_s=0,mass_kg=0,kinetic_j=0,external_work_j=0;Vec3 momentum;std::map<std::uint32_t,double> group_mass;std::vector<ParticleId> ids;};
// q=1, same SI frame and clock, one owner per ID. Contact-free seams only.
// This is a conservative reference connector, not a scalable domain-decomposition solver.
class ProcessChain {
 struct State {std::vector<ProcessRegion> regions;std::map<ParticleId,Residence> residence;std::vector<HandoffReceipt> receipts;std::uint64_t sequence=0;};
 std::shared_ptr<State> state_;
 static void need(bool b,const char* s){if(!b)throw std::invalid_argument(s);}
 static double radius(const ClumpBody& b){need(b.pebbles.size()==1&&b.pebbles[0].local_m.x==0&&b.pebbles[0].local_m.y==0&&b.pebbles[0].local_m.z==0,"process connector requires centered q=1 spheres");return b.pebbles[0].diameter_m/2;}
 static void seams(const State& s){for(std::size_t i=0;i<s.regions.size();++i)for(std::size_t j=i+1;j<s.regions.size();++j)for(const auto& a:s.regions[i].world.bodies)for(const auto& b:s.regions[j].world.bodies){auto x=a.position_m,y=b.position_m;need(std::hypot(x.x-y.x,x.y-y.y,x.z-y.z)>1.01*(radius(a)+radius(b)),"cross-region near contact: join regions before advancing");}}
 static void free_boundary(const ClumpWorld& w){need(w.histories()==0,"handoff requires no active region contact histories");if(w.bodies.empty())return;auto probe=w;const auto report=probe.step(1e-12);need(report.pair_contacts==0&&report.wall_contacts==0,"handoff boundary is in contact");}
public:
 explicit ProcessChain(std::vector<ProcessRegion> regions):state_(std::make_shared<State>()){
 need(regions.size()==5,"five process stages required");for(std::size_t i=0;i<regions.size();++i){auto& w=regions[i].world;need(static_cast<std::size_t>(regions[i].stage)==i,"process stage order must be supply/storage/transfer/receiver/mixing");need(w.physical_time_s==0,"initial process clock must be zero");for(const auto& b:w.bodies){radius(b);clump_properties(b.pebbles,b.density_kg_m3);need(state_->residence.emplace(b.id,Residence{i,0,{}}).second,"duplicate process ID");}}state_->regions=std::move(regions);seams(*state_);
 }
 const std::vector<ProcessRegion>& regions()const{return state_->regions;}
 const std::map<ParticleId,Residence>& residence()const{return state_->residence;}
 const std::vector<HandoffReceipt>& receipts()const{return state_->receipts;}
 double time_s()const{return state_->regions[0].world.physical_time_s;}
 void advance(double dt){need(std::isfinite(dt)&&dt>0&&std::isfinite(time_s()+dt),"invalid process dt");auto next=std::make_shared<State>(*state_);seams(*next);for(auto& r:next->regions){if(r.world.bodies.empty())r.world.physical_time_s+=dt;else r.world.step(dt);}seams(*next);for(std::size_t r=0;r<5;++r){const auto& before=state_->regions[r].world.bodies;const auto& after=next->regions[r].world.bodies;for(std::size_t i=0;i<before.size();++i){const auto a=before[i].position_m,b=after[i].position_m;need(std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<=.001*radius(before[i]),"process seam travel bound exceeded");}}for(auto& e:next->residence)e.second.seconds[e.second.owner]+=dt;state_=std::move(next);}
 // Caller supplies expected sequence and physical time: replay and stale packets fail atomically.
 HandoffReceipt handoff(std::size_t from,std::size_t to,const std::vector<ParticleId>& ids,double at,std::uint64_t sequence){
 need(from<5&&to==from+1&&to<5,"only adjacent forward handoff supported");need(sequence==state_->sequence+1,"stale/replayed handoff sequence");need(std::isfinite(at)&&at==time_s(),"handoff time differs from synchronized clock");need(!ids.empty(),"empty handoff");std::set<ParticleId> selected(ids.begin(),ids.end());need(selected.size()==ids.size(),"duplicate handoff IDs");for(auto id:ids){auto it=state_->residence.find(id);need(it!=state_->residence.end()&&it->second.owner==from,"handoff sender does not own ID");}
 const auto& oldfrom=state_->regions[from].world;const auto& oldto=state_->regions[to].world;need(oldfrom.physical_time_s==at&&oldto.physical_time_s==at,"region clock mismatch");need(oldfrom.pair_models.size()==oldto.pair_models.size(),"contact material contract mismatch");
 // Exact serialized material comparison is not inferred: require every effective model field.
 auto model_equal=[](const MaterialContact& a,const MaterialContact& b){std::ostringstream x,y;x<<std::setprecision(17);y<<std::setprecision(17);auto put=[](auto& o,const auto& m){o<<int(m.law)<<' '<<m.linear.kn<<' '<<m.linear.cn<<' '<<m.linear.kt<<' '<<m.linear.ct<<' '<<m.linear.mu<<' '<<m.a.young_pa<<' '<<m.a.poisson<<' '<<m.b.young_pa<<' '<<m.b.poisson<<' '<<m.cn<<' '<<m.ct<<' '<<m.mu<<' '<<m.rolling.mu<<' '<<m.adhesion_gamma_j_m2<<' '<<m.max_overlap_radius_ratio<<' '<<m.rolling.stiffness_n_m<<' '<<m.rolling.damping_ns_m<<' '<<m.linear.rolling.mu<<' '<<m.linear.rolling.stiffness_n_m<<' '<<m.linear.rolling.damping_ns_m<<' '<<m.linear.adhesion_gamma_j_m2;};put(x,a);put(y,b);return x.str()==y.str();};
 for(const auto& e:oldfrom.pair_models){auto it=oldto.pair_models.find(e.first);need(it!=oldto.pair_models.end()&&model_equal(e.second,it->second),"pair law changes at handoff");}
 free_boundary(oldfrom);free_boundary(oldto);auto next=std::make_shared<State>(*state_);auto& a=next->regions[from].world;auto& b=next->regions[to].world;HandoffReceipt receipt;receipt.sequence=sequence;receipt.from=from;receipt.to=to;receipt.time_s=at;receipt.ids=ids;
 for(const auto& body:a.bodies)if(selected.count(body.id)){double mass=clump_properties(body.pebbles,body.density_kg_m3).mass_kg;receipt.mass_kg+=mass;receipt.group_mass[body.type]+=mass;receipt.kinetic_j+=clump_kinetic_energy(body);receipt.momentum.x+=mass*body.velocity_m_s.x;receipt.momentum.y+=mass*body.velocity_m_s.y;receipt.momentum.z+=mass*body.velocity_m_s.z;b.bodies.push_back(body);next->residence.at(body.id).owner=to;next->residence.at(body.id).entered_s=at;}
 a.bodies.erase(std::remove_if(a.bodies.begin(),a.bodies.end(),[&](const auto& body){return selected.count(body.id)!=0;}),a.bodies.end());a.invalidate_caches();b.invalidate_caches();free_boundary(b);seams(*next);next->sequence=sequence;next->receipts.push_back(receipt);state_=std::move(next);return receipt;
 }
};
}
