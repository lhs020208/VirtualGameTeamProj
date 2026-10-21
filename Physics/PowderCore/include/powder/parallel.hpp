#pragma once
#include <algorithm>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
namespace powder {
// Single caller per executor, as for ClumpWorld::step. Copies get an independent,
// lazy pool. Workers never touch engine objects or the committed physical state.
class ParallelExecutor {
 struct Pool {
  std::mutex mutex;
  std::condition_variable ready,done;
  bool stop=false;
  std::size_t generation=0,pending=0,count=0;
  unsigned lanes;
  std::function<void(std::size_t)> job;
  std::vector<std::thread> workers;
  std::vector<std::exception_ptr> errors;
  explicit Pool(unsigned n):lanes(n),errors(n){
   try {for(unsigned lane=1;lane<n;++lane)workers.emplace_back([this,lane]{
    std::size_t seen=0;
    for(;;){std::unique_lock<std::mutex> lock(mutex);
     ready.wait(lock,[&]{return stop||generation!=seen;});if(stop)return;
     seen=generation;lock.unlock();execute(lane);lock.lock();
     if(--pending==0)done.notify_one();
    }
   });}catch(...){shutdown();throw;}
  }
  void shutdown(){ {std::lock_guard<std::mutex> lock(mutex);stop=true;}ready.notify_all();for(auto& t:workers)if(t.joinable())t.join(); }
  ~Pool(){shutdown();}
  void execute(unsigned lane)noexcept{
   try{for(std::size_t i=count*lane/lanes;i<count*(lane+1)/lanes;++i)job(i);}
   catch(...){errors[lane]=std::current_exception();}
  }
  void run(std::size_t n,const std::function<void(std::size_t)>& f){
   {std::lock_guard<std::mutex> lock(mutex);job=f;count=n;std::fill(errors.begin(),errors.end(),nullptr);pending=lanes-1;++generation;}
   ready.notify_all();execute(0);
   {std::unique_lock<std::mutex> lock(mutex);done.wait(lock,[&]{return pending==0;});job={};}
   // Static contiguous ranges make the first failing lane the lowest failing index.
   for(const auto& error:errors)if(error)std::rethrow_exception(error);
  }
 };
 std::unique_ptr<Pool> pool_;
public:
 ParallelExecutor()=default;
 ParallelExecutor(const ParallelExecutor&){}
 ParallelExecutor& operator=(const ParallelExecutor& other){if(this!=&other)pool_.reset();return *this;}
 ParallelExecutor(ParallelExecutor&&)noexcept=default;
 ParallelExecutor& operator=(ParallelExecutor&&)noexcept=default;
 static void validate(unsigned threads,std::size_t minimum){if(threads<1||threads>32||minimum<1)throw std::invalid_argument("CPU threads must be 1..32 and minimum jobs >= 1");}
 static unsigned lanes(std::size_t count,unsigned threads,std::size_t minimum){validate(threads,minimum);return count<minimum?1:static_cast<unsigned>(std::min<std::size_t>(count?count:1,threads));}
 template<class F>unsigned run(std::size_t count,unsigned threads,std::size_t minimum,const F& f){
  const auto n=lanes(count,threads,minimum);
  if(n==1){if(threads==1)pool_.reset();for(std::size_t i=0;i<count;++i)f(i);return 1;}
  if(!pool_||pool_->lanes!=n)pool_=std::make_unique<Pool>(n);
  pool_->run(count,f);return n;
 }
 unsigned resident_workers()const{return pool_?pool_->lanes-1:0;}
};
}
