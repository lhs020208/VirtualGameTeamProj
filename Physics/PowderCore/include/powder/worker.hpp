#pragma once
#include "powder/session.hpp"
#include "powder/tracking.hpp"
#include "powder/results.hpp"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
namespace powder {
struct Snapshot {
 std::uint64_t generation=0,steps=0;
 bool ready=false,running=false;
 double simulation_seconds=0,compute_seconds=0;
 std::vector<Particle> particles;
 StepReport report;
 TrackingSummary tracking;
 ResultState results;
 double dt_s=0;
 std::string measurement="{}",geometry="{}";
 InitialReport initial;
 SupplyReport supply;
 std::string settings_text;
 std::size_t histories=0;
 std::string error;
};
// Owns one solver thread. No engine objects/callbacks cross the boundary.
class Worker {
public:
 Worker();
 ~Worker();
 Worker(const Worker&)=delete;
 Worker& operator=(const Worker&)=delete;
 std::uint64_t reset(ParticleSystem world,double dt,TrackingLayout layout={},ResultCriteria criteria={});
 std::uint64_t configure(PowderSettings settings,ParticleSystem geometry,TrackingLayout layout={},ResultCriteria criteria={});
 void toggle();
 void pause();
 void single_step();
 std::shared_ptr<const Snapshot> snapshot()const;
private:
 struct Reset {ResultCriteria criteria;std::optional<PowderSettings> settings;ParticleSystem world;TrackingLayout layout;double dt;std::uint64_t generation;};
 std::uint64_t enqueue(std::unique_ptr<Reset> reset);
 void loop();
 mutable std::mutex mutex_;
 std::condition_variable cv_;
 bool stop_=false,run_=false;
 std::uint64_t generation_=0,revision_=0,pending_steps_=0;
 std::unique_ptr<Reset> reset_;
 std::shared_ptr<const Snapshot> latest_;
 std::thread thread_;
};
}
