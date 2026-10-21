#include "powder/clump_worker.hpp"
namespace powder {
void ClumpWorkerOptions::validate()const {
 if(!std::isfinite(target_rate)||target_rate<=0||target_rate>10||!std::isfinite(snapshot_hz)||snapshot_hz<1||snapshot_hz>120||!std::isfinite(batch_budget_ms)||batch_budget_ms<1||batch_budget_ms>100||batch_steps<1||batch_steps>5000)
  throw std::invalid_argument("invalid worker pacing/snapshot/batch options");
}
ClumpWorker::ClumpWorker(ClumpRunState state,ClumpWorkerOptions options){
 options.validate();thread_=std::thread(&ClumpWorker::loop,this);
 try{reset(std::move(state),options);}catch(...){ {std::lock_guard<std::mutex> lock(mutex_);stopping_=true;}cv_.notify_one();thread_.join();throw;}
}
ClumpWorker::~ClumpWorker(){ {std::lock_guard<std::mutex> lock(mutex_);stopping_=true;}cv_.notify_one();if(thread_.joinable())thread_.join();}
void ClumpWorker::submit(std::function<void()> command){
 auto task=std::make_shared<std::packaged_task<void()>>(std::move(command));auto done=task->get_future();
 {std::lock_guard<std::mutex> lock(mutex_);if(stopping_)throw std::runtime_error("worker stopped");commands_.emplace_back([task]{(*task)();});}
 cv_.notify_one();done.get();
}
void ClumpWorker::reset(ClumpRunState state,ClumpWorkerOptions options){
 options.validate();
 submit([this,state=std::move(state),options]()mutable{
  if(!std::isfinite(state.Dt)||state.Dt<=0)throw std::invalid_argument("invalid worker fixed dt");
  // Probe before adopting the reset; failure leaves the previous generation intact.
  auto probe=state.World;probe.step(state.Dt);
  state_=std::move(state);options_=options;running_=false;++generation_;sequence_=publications_=0;snapshot_seconds_=actual_rate_=0;
  run_start_=rate_start_=Clock::now();run_origin_s_=rate_origin_s_=state_.World.physical_time_s;publish();
 });
}
bool ClumpWorker::finished()const{return state_.Mixing&&state_.Mixing->finished(state_.Steps);}
void ClumpWorker::set_options(ClumpWorkerOptions options){options.validate();submit([this,options]{if(running_)throw std::logic_error("pause before changing worker options");options_=options;});}
void ClumpWorker::run(){submit([this]{if(!state_.Error.empty()||finished())return;if(!running_){running_=true;run_start_=rate_start_=Clock::now();run_origin_s_=rate_origin_s_=state_.World.physical_time_s;actual_rate_=0;}publish();});}
void ClumpWorker::pause(){submit([this]{running_=false;publish();});}
void ClumpWorker::single_step(){submit([this]{running_=false;advance();publish();});}
ClumpRunState ClumpWorker::checkpoint(){ClumpRunState result;submit([this,&result]{if(running_)throw std::logic_error("pause before checkpoint");result=state_;});return result;}
std::shared_ptr<const ClumpSnapshot> ClumpWorker::snapshot()const{std::lock_guard<std::mutex> lock(mutex_);return latest_;}
void ClumpWorker::advance(){
 if(!state_.Error.empty()||finished()){running_=false;return;}
 const auto start=Clock::now();
 try{const double load_time=state_.World.physical_time_s;state_.Report=state_.World.step_profiled(state_.Dt,options_.profile?&state_.Profile:nullptr);state_.LoadTime=load_time;++state_.Steps;
  if(state_.Mixing)state_.Mixing->record(state_.World.bodies,state_.Steps);
  if(finished())running_=false;
 }catch(const std::exception& e){state_.Error=e.what();running_=false;}
 state_.ComputeSeconds+=std::chrono::duration<double>(Clock::now()-start).count();
}
void ClumpWorker::publish(){
 const auto start=Clock::now();auto s=std::make_shared<ClumpSnapshot>();
 s->generation=generation_;s->sequence=++sequence_;s->steps=state_.Steps;s->publications=++publications_;s->running=running_;s->finished=finished();
 s->time_s=state_.World.physical_time_s;s->load_time_s=state_.LoadTime;s->wall_work_j=state_.World.moving_wall_work_j;
 s->compute_s=state_.ComputeSeconds;s->bodies=state_.World.bodies;s->report=state_.Report;s->profile=state_.Profile;s->error=state_.Error;
 if(state_.Mixing){s->sample_count=state_.Mixing->samples.size();if(!state_.Mixing->samples.empty())s->mixing=state_.Mixing->samples.back();}
 const double window=std::chrono::duration<double>(start-rate_start_).count();
 if(window>=.5){actual_rate_=(s->time_s-rate_origin_s_)/window;rate_start_=start;rate_origin_s_=s->time_s;}
 s->actual_rate=actual_rate_;
 s->lag_s=running_&&options_.paced?std::max(0.,run_origin_s_+std::chrono::duration<double>(start-run_start_).count()*options_.target_rate-s->time_s):0;
 snapshot_seconds_+=std::chrono::duration<double>(Clock::now()-start).count();s->snapshot_s=snapshot_seconds_;
 {std::lock_guard<std::mutex> lock(mutex_);latest_=std::move(s);}last_publish_=start;
}
void ClumpWorker::loop(){
 for(;;){
  std::function<void()> command;
  {std::unique_lock<std::mutex> lock(mutex_);if(!running_)cv_.wait(lock,[this]{return stopping_||!commands_.empty();});if(stopping_)return;
   if(!commands_.empty()){command=std::move(commands_.front());commands_.pop_front();}}
  if(command){command();continue;}
  const auto batch_start=Clock::now();
  for(std::size_t i=0;i<options_.batch_steps&&running_;++i){
   {std::lock_guard<std::mutex> lock(mutex_);if(stopping_||!commands_.empty())break;}
   const auto now=Clock::now();
   if(options_.paced){
    const double target=run_origin_s_+std::chrono::duration<double>(now-run_start_).count()*options_.target_rate;
    if(state_.World.physical_time_s+state_.Dt>target){
     const auto delay=std::chrono::duration<double>((state_.World.physical_time_s+state_.Dt-target)/options_.target_rate);
     std::unique_lock<std::mutex> lock(mutex_);cv_.wait_for(lock,std::min(delay,std::chrono::duration<double>(.01)),[this]{return stopping_||!commands_.empty();});break;
    }
   }
   advance();
   if(!running_||std::chrono::duration<double>(Clock::now()-last_publish_).count()>=1/options_.snapshot_hz)publish();
   if(std::chrono::duration<double,std::milli>(Clock::now()-batch_start).count()>=options_.batch_budget_ms)break;
  }
  // A batch boundary yields scheduling, never changes the fixed physics step.
  std::this_thread::yield();
 }
}
}
