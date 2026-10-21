#include "powder/results.hpp"
#include "powder/worker.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
namespace powder {
namespace {
std::ostringstream stream(){std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17);return o;}
std::string quote(const std::string& s){auto o=stream();o<<'"';for(unsigned char c:s){switch(c){case '"':o<<"\\\"";break;case '\\':o<<"\\\\";break;case '\n':o<<"\\n";break;case '\r':o<<"\\r";break;case '\t':o<<"\\t";break;default:if(c<32)o<<"\\u00"<<std::hex<<std::setw(2)<<std::setfill('0')<<int(c)<<std::dec;else o<<char(c);}}o<<'"';return o.str();}
void vec(std::ostream& o,Vec3 v){o<<'['<<v.x<<','<<v.y<<','<<v.z<<']';}
const char* region(Location l){const char* names[]={"feed","tube","collector","outside"};return names[static_cast<unsigned>(l)];}
double speed(const Particle& p){return std::hypot(p.velocity_m_s.x,p.velocity_m_s.y,p.velocity_m_s.z);}
}
void validate_criteria(const ResultCriteria& c){
 if(!std::isfinite(c.sample_s)||c.sample_s<=0||!std::isfinite(c.slow_speed_m_s)||c.slow_speed_m_s<0||!std::isfinite(c.long_stall_s)||c.long_stall_s<=0||c.max_samples<2||c.max_samples>100000)throw std::invalid_argument("invalid result criteria: positive sample/stall time, nonnegative speed, 2..100000 samples");
}
ResultMonitor::ResultMonitor(ResultCriteria c){validate_criteria(c);state_.criteria=c;state_.samples=std::make_shared<const std::vector<TimeSample>>();}
void ResultMonitor::observe(const std::vector<Particle>& particles,const TrackingSummary& tracking,const SupplyReport& supply,double time){
 if(!std::isfinite(time)||time<0||(previous_>=0&&time<=previous_))throw std::invalid_argument("result observation time must increase");
 if(tracking.total_count!=particles.size())throw std::invalid_argument("result tracking/world count mismatch");
 auto& row=state_.current;double last_exit=-1;row.time_s=time;row.counts=tracking.counts;row.masses=tracking.mass_kg;row.unique_pass_kg=tracking.unique_passed_mass_kg;row.supply=supply;row.slow_count=row.long_slow_count=0;
 for(const auto& p:particles){auto it=state_.slow_seconds.find(p.id);double duration=0;
  if(speed(p)<=state_.criteria.slow_speed_m_s){++row.slow_count;if(it!=state_.slow_seconds.end()&&was_slow_[p.id])duration=it->second+(previous_<0?0:time-previous_);}
  was_slow_[p.id]=speed(p)<=state_.criteria.slow_speed_m_s;state_.slow_seconds[p.id]=duration;if(duration>=state_.criteria.long_stall_s)++row.long_slow_count;
 }
 for(const auto& rec:tracking.records){auto& n=seen_[rec.id];if(n>rec.events.size())throw std::invalid_argument("result tracking history shrank");
  for(;n<rec.events.size();++n){const auto& e=rec.events[n];if(e.kind==Crossing::ExitDown){row.gross_exit_kg+=rec.mass_kg;row.net_exit_kg+=rec.mass_kg;last_exit=std::max(last_exit,e.time_s);}else if(e.kind==Crossing::ExitUp)row.net_exit_kg-=rec.mass_kg;}
 }
 const bool upstream=tracking.counts[0]+tracking.counts[1]+supply.pending>0;
 if(!tracking.enabled||!upstream){no_outflow_start_=-1;row.no_outflow_s=0;row.flow_state=tracking.enabled?"no_upstream_material":"tracking_disabled";}
 else {if(no_outflow_start_<0)no_outflow_start_=time;if(last_exit>=0)no_outflow_start_=last_exit;row.no_outflow_s=time-no_outflow_start_;row.flow_state=row.no_outflow_s>=state_.criteria.long_stall_s?"prolonged_no_outflow":last_exit>=0?"outflow_observed":"temporary_no_outflow";}
 row.supply_state=!supply.configured?"unconfigured":supply.complete?"complete":supply.pending?"backlogged":supply.requested?"feeding":"scheduled";
 if(time>=next_sample_){
  if(state_.samples->size()>=state_.criteria.max_samples){state_.sample_limit_reached=true;previous_=time;return;}
  auto next=std::make_shared<std::vector<TimeSample>>(*state_.samples);next->push_back(row);state_.samples=std::move(next);next_sample_=(std::floor(time/state_.criteria.sample_s)+1)*state_.criteria.sample_s;
 }previous_=time;
}
std::string measurement_json(const TrackingLayout& l){auto o=stream();o<<"{\"enabled\":"<<(l.enabled?"true":"false")<<",\"axis\":";vec(o,l.axis);o<<",\"bottom\":"<<l.bottom<<",\"top\":"<<l.top;
 const char* names[]={"bore","feed","funnel","collector"};const Volume* volumes[]={&l.bore,&l.feed,&l.funnel,&l.collector};for(int k=0;k<4;++k){o<<",\""<<names[k]<<"\":[";bool first=true;for(auto b:volumes[k]->sides){if(!first)o<<',';first=false;o<<'[';vec(o,b.normal);o<<','<<b.offset<<']';}o<<']';}o<<'}';return o.str();}
std::string geometry_json(const ParticleSystem& w){auto o=stream();o<<"{\"boxes\":[";bool first=true;for(auto& b:w.boxes){if(!first)o<<',';first=false;o<<"{\"id\":"<<b.id<<",\"weld_group\":"<<b.weld_group<<",\"centre_m\":";vec(o,b.centre_m);o<<",\"half_m\":";vec(o,b.half_m);o<<",\"axes\":[";for(int k=0;k<3;++k){if(k)o<<',';vec(o,b.axes[k]);}o<<"]}";}o<<"],\"planes\":[";first=true;for(auto& p:w.planes){if(!first)o<<',';first=false;o<<"{\"id\":"<<p.id<<",\"normal\":";vec(o,p.normal);o<<",\"offset_m\":"<<p.offset_m<<'}';}o<<"]}";return o.str();}
std::map<std::string,std::string> export_results(const Snapshot& s){
 if(!s.ready||s.running||!s.error.empty()||!s.results.samples||s.results.samples->empty())throw std::invalid_argument("results require ready, paused, error-free recorded run");
 if(s.results.current.time_s!=s.simulation_seconds)throw std::invalid_argument("result clock mismatch");
 const auto& r=s.results;const auto& c=r.criteria;const auto& current=r.current;std::map<std::string,std::string> files;
 auto meta=stream();meta<<"{\n\"schema\":1,\"metric_definition\":\"dem_results_v1\",\"code_version\":"<<quote(Version)<<",\"build_stamp\":"<<quote(__DATE__ " " __TIME__)<<",\"sample_limit_reached\":"<<(r.sample_limit_reached?"true":"false")<<",\"source_revision\":null,\"generation\":"<<s.generation<<",\"steps\":"<<s.steps<<",\"physical_time_s\":"<<s.simulation_seconds<<",\"dt_s\":"<<s.dt_s<<",\"seed\":"<<s.initial.seed<<",\"compute_seconds\":"<<s.compute_seconds<<",\"settings\":"<<quote(s.settings_text)<<",\"criteria\":{\"sample_s\":"<<c.sample_s<<",\"slow_speed_m_s\":"<<c.slow_speed_m_s<<",\"long_stall_s\":"<<c.long_stall_s<<",\"max_samples\":"<<c.max_samples<<"},\"measurement\":"<<s.measurement<<",\"geometry\":"<<s.geometry<<"}\n";files["metadata.json"]=meta.str();files["settings.dem"]=s.settings_text;
 auto csv=stream();csv<<"time_s,gross_exit_kg,net_exit_kg,interval_exit_kg,interval_exit_kg_s,unique_pass_kg,feed_n,tube_n,collector_n,outside_n,feed_kg,tube_kg,collector_kg,outside_kg,planned_n,requested_n,inserted_n,pending_n,future_n,planned_kg,requested_kg,inserted_kg,pending_kg,future_kg,supply_balance_kg,no_outflow_s,slow_n,long_slow_n,flow_state,supply_state\n";
 auto samples=*r.samples;if(samples.back().time_s!=current.time_s)samples.push_back(current);
 double prev_t=0,prev_m=0;for(const auto& row:samples){double delta=row.gross_exit_kg-prev_m,dt=row.time_s-prev_t;csv<<row.time_s<<','<<row.gross_exit_kg<<','<<row.net_exit_kg<<','<<delta<<',';if(dt>0)csv<<delta/dt;csv<<','<<row.unique_pass_kg;for(auto n:row.counts)csv<<','<<n;for(auto m:row.masses)csv<<','<<m;
 const auto& q=row.supply;csv<<','<<q.planned<<','<<q.requested<<','<<q.inserted<<','<<q.pending<<','<<q.future<<','<<q.planned_mass<<','<<q.requested_mass<<','<<q.inserted_mass<<','<<q.pending_mass<<','<<q.future_mass<<','<<q.mass_error<<','<<row.no_outflow_s<<','<<row.slow_count<<','<<row.long_slow_count<<','<<row.flow_state<<','<<row.supply_state<<'\n';prev_t=row.time_s;prev_m=row.gross_exit_kg;}
 files["timeseries.csv"]=csv.str();
 auto particles=stream();particles<<"id,location,mass_kg,x_m,y_m,z_m,vx_m_s,vy_m_s,vz_m_s,speed_m_s,birth_s,age_s,location_since_s,slow_duration_s,slow_state,ever_passed,first_entry_s,first_exit_s\n";
 auto events=stream();events<<"id,kind,time_s,mass_kg,completed_passage,residence_s\n";std::vector<double> residence;
 std::map<ParticleId,const Particle*> lookup;for(auto& p:s.particles)lookup[p.id]=&p;
 for(auto& track:s.tracking.records){auto it=lookup.find(track.id);if(it==lookup.end())throw std::invalid_argument("result particle ID mismatch");const auto& p=*it->second;auto low=r.slow_seconds.at(p.id);const char* low_state=speed(p)>c.slow_speed_m_s?"moving":low>=c.long_stall_s?"long_slow":"temporary_slow";
 particles<<p.id<<','<<region(track.location)<<','<<track.mass_kg<<','<<p.position_m.x<<','<<p.position_m.y<<','<<p.position_m.z<<','<<p.velocity_m_s.x<<','<<p.velocity_m_s.y<<','<<p.velocity_m_s.z<<','<<speed(p)<<','<<track.birth_time_s<<','<<s.simulation_seconds-track.birth_time_s<<','<<track.location_since_s<<','<<low<<','<<low_state<<','<<track.ever_passed<<','<<track.first_entry_s<<','<<track.first_exit_s<<'\n';
 double entry=-1;for(auto& e:track.events){if(e.kind==Crossing::EntryDown)entry=e.time_s;events<<p.id<<','<<static_cast<unsigned>(e.kind)<<','<<e.time_s<<','<<track.mass_kg<<','<<e.completed_passage<<',';if(e.completed_passage){if(entry<0||e.time_s<entry)throw std::invalid_argument("invalid passage residence history");double d=e.time_s-entry;residence.push_back(d);events<<d;}events<<'\n';}
 }files["particles.csv"]=particles.str();files["events.csv"]=events.str();
 std::sort(residence.begin(),residence.end());double sum=0;for(auto t:residence)sum+=t;
 auto summary=stream();summary<<"{\"schema\":1,\"physical_time_s\":"<<s.simulation_seconds<<",\"tracking_enabled\":"<<(s.tracking.enabled?"true":"false")<<",\"gross_exit_kg\":"<<current.gross_exit_kg<<",\"net_exit_kg\":"<<current.net_exit_kg<<",\"unique_pass_kg\":"<<current.unique_pass_kg<<",\"passages\":"<<s.tracking.passages<<",\"retained_feed_tube_kg\":"<<s.tracking.mass_kg[0]+s.tracking.mass_kg[1]<<",\"outside_kg\":"<<s.tracking.mass_kg[3]<<",\"collector_kg\":"<<s.tracking.mass_kg[2]<<",\"pending_kg\":"<<s.supply.pending_mass<<",\"future_kg\":"<<s.supply.future_mass<<",\"inserted_kg\":"<<s.supply.inserted_mass<<",\"tracking_balance_kg\":"<<s.tracking.mass_error_kg<<",\"supply_balance_kg\":"<<s.supply.mass_error<<",\"no_outflow_s\":"<<current.no_outflow_s<<",\"slow_n\":"<<current.slow_count<<",\"long_slow_n\":"<<current.long_slow_count<<",\"flow_state\":"<<quote(current.flow_state)<<",\"supply_state\":"<<quote(current.supply_state)<<",\"residence_count\":"<<residence.size()<<",\"residence_mean_s\":";
 if(residence.empty())summary<<"null";else summary<<sum/residence.size();summary<<",\"residence_median_s\":";if(residence.empty())summary<<"null";else {auto n=residence.size();summary<<(n%2?residence[n/2]:(residence[n/2-1]+residence[n/2])/2);}summary<<",\"residence_samples_s\":[";for(std::size_t i=0;i<residence.size();++i){if(i)summary<<',';summary<<residence[i];}summary<<"]}\n";files["summary.json"]=summary.str();return files;
}
}
