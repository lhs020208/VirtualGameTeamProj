#pragma once
#include "powder/mixing.hpp"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <thread>

namespace powder {
class ClumpBackend {
public:
 virtual ~ClumpBackend()=default;
 virtual std::shared_ptr<ClumpBackend> clone()=0;
 virtual void advance(ClumpWorld&,double)=0;
 virtual void synchronize(ClumpWorld&,ClumpReport&)=0;
};
// All mutable solver/measurement state belongs to one thread. No engine types.
struct ClumpRunState {
 ClumpWorld World;
 std::shared_ptr<ClumpBackend> Backend;
 ClumpProfile Profile;
 double Dt=2e-6, LoadTime=0, ComputeSeconds=0;
 std::uint64_t Steps=0;
 std::optional<MixingMonitor> Mixing;
 ClumpReport Report;
 std::string Error;
};
struct ClumpWorkerOptions {
 bool paced=true, profile=false;
 double target_rate=1, snapshot_hz=30, batch_budget_ms=50;
 std::size_t batch_steps=200;
 void validate()const;
};
struct ClumpSnapshot {
 std::uint64_t generation=0, sequence=0, steps=0, publications=0;
 bool running=false, finished=false;
 double time_s=0, load_time_s=0, wall_work_j=0, compute_s=0, snapshot_s=0, actual_rate=0, lag_s=0;
 std::vector<ClumpBody> bodies;
 ClumpReport report;
 ClumpProfile profile;
 std::optional<MixingSample> mixing; // Last physical-time sample, NOT a render-time measurement.
 std::size_t sample_count=0;
 std::string error;
};
class ClumpWorker {
public:
 ClumpWorker(ClumpRunState state,ClumpWorkerOptions options={});
 ~ClumpWorker();
 ClumpWorker(const ClumpWorker&)=delete;
 ClumpWorker& operator=(const ClumpWorker&)=delete;
 // Controls are barriers: when returned, the preceding in-flight step is complete.
 void reset(ClumpRunState state,ClumpWorkerOptions options={});
 void run();
 void set_options(ClumpWorkerOptions options);
 void pause();
 void single_step();
 ClumpRunState checkpoint(); // Rejected while running; full history copied only on explicit export.
 std::shared_ptr<const ClumpSnapshot> snapshot()const;
private:
 using Clock=std::chrono::steady_clock;
 void submit(std::function<void()> command);
 void loop();
 void advance();
 void publish();
 bool finished()const;
 ClumpRunState state_;
 ClumpWorkerOptions options_;
 bool running_=false;
 std::uint64_t generation_=0,sequence_=0,publications_=0;
 double snapshot_seconds_=0,run_origin_s_=0,rate_origin_s_=0,actual_rate_=0;
 Clock::time_point run_start_,rate_start_,last_publish_;
 mutable std::mutex mutex_;
 std::condition_variable cv_;
 bool stopping_=false;
 std::deque<std::function<void()>> commands_;
 std::shared_ptr<const ClumpSnapshot> latest_;
 std::thread thread_;
};
}
