#include "powder/worker.hpp"
#include <chrono>
namespace powder {
Worker::Worker(){latest_=std::make_shared<Snapshot>();thread_=std::thread(&Worker::loop,this);}
Worker::~Worker(){ {std::lock_guard<std::mutex> lock(mutex_);stop_=true;}cv_.notify_one();if(thread_.joinable())thread_.join();}
std::uint64_t Worker::reset(ParticleSystem world,double dt,TrackingLayout layout,ResultCriteria criteria){
 auto r=std::make_unique<Reset>();r->criteria=criteria;r->world=std::move(world);r->dt=dt;r->layout=std::move(layout);
 return enqueue(std::move(r));
}
std::uint64_t Worker::configure(PowderSettings settings,ParticleSystem world,TrackingLayout layout,ResultCriteria criteria){
 auto r=std::make_unique<Reset>();r->criteria=criteria;r->dt=settings.dt_s;r->settings=std::move(settings);r->world=std::move(world);r->layout=std::move(layout);return enqueue(std::move(r));
}
std::uint64_t Worker::enqueue(std::unique_ptr<Reset> r){
 std::lock_guard<std::mutex> lock(mutex_);r->generation=++generation_;reset_=std::move(r);run_=false;pending_steps_=0;++revision_;
 auto loading=std::make_shared<Snapshot>();loading->generation=generation_;latest_=loading;cv_.notify_one();return generation_;
}
void Worker::toggle(){std::lock_guard<std::mutex> lock(mutex_);if(latest_->error.empty()&&!latest_->results.sample_limit_reached){run_=!run_;pending_steps_=0;++revision_;cv_.notify_one();}}
void Worker::pause(){std::lock_guard<std::mutex> lock(mutex_);run_=false;pending_steps_=0;++revision_;cv_.notify_one();}
void Worker::single_step(){std::lock_guard<std::mutex> lock(mutex_);if(latest_->error.empty()&&!latest_->results.sample_limit_reached){run_=false;++pending_steps_;++revision_;cv_.notify_one();}}
std::shared_ptr<const Snapshot> Worker::snapshot()const{std::lock_guard<std::mutex> lock(mutex_);return latest_;}
void Worker::loop(){
 std::unique_ptr<ResultMonitor> monitor;std::string measurement="{}",geometry="{}";
 std::unique_ptr<Supply> supply;
 InitialReport initial;std::string settings_text;Tracker tracker;std::string tracking_failure;std::unique_ptr<Session> session;std::uint64_t seen=0,gen=0;double compute=0;unsigned since_publish=0;
 for(;;){
  std::unique_ptr<Reset> reset;bool step=false,single=false;
  {std::unique_lock<std::mutex> lock(mutex_);cv_.wait(lock,[&]{return stop_||reset_||run_||pending_steps_||revision_!=seen;});
   if(stop_)return;seen=revision_;
   if(reset_)reset=std::move(reset_);
   else{step=run_||pending_steps_>0;single=pending_steps_>0;if(single)--pending_steps_;}
  }
  std::string failure=tracking_failure;
  if(reset){
   gen=reset->generation;compute=0;since_publish=0;session.reset();supply.reset();monitor.reset();tracking_failure.clear();failure.clear();tracker=Tracker{};initial={};settings_text.clear();
   try{
    monitor=std::make_unique<ResultMonitor>(reset->criteria);measurement=measurement_json(reset->layout);geometry=geometry_json(reset->world);
    if(reset->settings){
     supply=std::make_unique<Supply>(*reset->settings,std::move(reset->world),[&]{std::lock_guard<std::mutex> lock(mutex_);return stop_||gen!=generation_;});
     reset->world=supply->empty_world();supply->poll(reset->world,0,[&]{std::lock_guard<std::mutex> lock(mutex_);return stop_||gen!=generation_;});initial=supply->initial();settings_text=serialize_settings(*reset->settings);
    }
    auto check=reset->world;check.step(reset->dt);tracker.reset(std::move(reset->layout),reset->world.particles);session=std::make_unique<Session>(std::move(reset->world),reset->dt);monitor->observe(session->world().particles,tracker.summary(),supply?supply->report():SupplyReport{},0);}
   catch(const std::exception& e){failure=e.what();}
  }else if(step&&session){
   const auto start=std::chrono::steady_clock::now();if(session->single_step()){try{tracker.advance(session->world().particles,session->simulation_seconds());
     if(supply&&supply->needs_poll(session->simulation_seconds())){
      auto next=*supply;auto world=session->world();auto born=next.poll(world,session->simulation_seconds(),[&]{std::lock_guard<std::mutex> lock(mutex_);return stop_||gen!=generation_;});
      tracker.admit(born,session->simulation_seconds());session->adopt_supply(std::move(world));*supply=std::move(next);
     }
     monitor->observe(session->world().particles,tracker.summary(),supply?supply->report():SupplyReport{},session->simulation_seconds());
     }catch(const std::exception& e){tracking_failure=e.what();failure=tracking_failure;}}
   compute+=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();++since_publish;
  }
  if(monitor&&monitor->state().sample_limit_reached){std::lock_guard<std::mutex> lock(mutex_);run_=false;pending_steps_=0;}
  if(session&&!session->error().empty())failure=session->error();
  bool publish=bool(reset)||!step||single||since_publish>=8||!failure.empty();
  {std::lock_guard<std::mutex> lock(mutex_);publish=publish||!run_;}
  if(!publish)continue;
  auto s=std::make_shared<Snapshot>();s->generation=gen;s->ready=true;s->compute_seconds=compute;s->error=failure;s->tracking=tracker.summary();s->initial=initial;if(supply)s->supply=supply->report();s->settings_text=settings_text;s->measurement=measurement;s->geometry=geometry;if(monitor)s->results=monitor->state();
  if(session){s->dt_s=session->dt();s->particles=session->world().particles;s->steps=session->steps();s->simulation_seconds=session->simulation_seconds();s->report=session->report();s->histories=session->world().active_pair_histories()+session->world().active_wall_histories();}
  {std::lock_guard<std::mutex> lock(mutex_);
   if(gen!=generation_)continue; // Never publish a completed step from a retired run.
   if(!failure.empty()){run_=false;pending_steps_=0;}s->running=run_;latest_=std::move(s);
  }
  since_publish=0;
 }
}
}
