#pragma once
#include "powder/tracking.hpp"
#include "powder/supply.hpp"
#include <memory>
namespace powder {
struct ResultCriteria {
 double sample_s=.01,slow_speed_m_s=.001,long_stall_s=1;
 std::size_t max_samples=10000;
};
void validate_criteria(const ResultCriteria&);
struct TimeSample {
 double time_s=0,gross_exit_kg=0,net_exit_kg=0,unique_pass_kg=0,no_outflow_s=0;
 std::array<std::size_t,4> counts{};std::array<double,4> masses{};
 SupplyReport supply;
 std::size_t slow_count=0,long_slow_count=0;
 std::string flow_state,supply_state;
};
struct ResultState {
 bool sample_limit_reached=false;
 ResultCriteria criteria;
 std::shared_ptr<const std::vector<TimeSample>> samples;
 TimeSample current;
 std::map<ParticleId,double> slow_seconds;
};
class ResultMonitor {
public:
 explicit ResultMonitor(ResultCriteria criteria={});
 void observe(const std::vector<Particle>&,const TrackingSummary&,const SupplyReport&,double time);
 const ResultState& state()const{return state_;}
private:
 std::map<ParticleId,bool> was_slow_;
 ResultState state_;std::map<ParticleId,std::size_t> seen_;
 double previous_=-1,next_sample_=0,no_outflow_start_=-1;
};
struct Snapshot;
// Pure, deterministic serialization. File publication belongs to the host.
std::map<std::string,std::string> export_results(const Snapshot&);
std::string measurement_json(const TrackingLayout&);
std::string geometry_json(const ParticleSystem&);
}
