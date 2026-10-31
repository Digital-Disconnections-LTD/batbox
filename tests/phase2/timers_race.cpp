#include "timers/Timers.hpp"
#include <atomic>
#include <iostream>
#include <thread>
using namespace batbox::timers;
namespace {
struct Handle:TimerHandle {std::atomic<int>* cancels;void cancel()override{++*cancels;}};
struct ImmediateScheduler:TimerScheduler {
  std::atomic<int> cancels{0};
  std::shared_ptr<TimerHandle> schedule(double,std::function<void()> callback,bool)override{
    callback();
    auto handle=std::make_shared<Handle>();handle->cancels=&cancels;return handle;
  }
};
struct OverlapScheduler:TimerScheduler {
  std::atomic<int> cancels{0};
  std::shared_ptr<TimerHandle> schedule(double,std::function<void()> callback,bool)override{
    std::thread worker([callback=std::move(callback)]{callback();});
    worker.join();
    auto handle=std::make_shared<Handle>();handle->cancels=&cancels;return handle;
  }
};
template<class Scheduler>bool check(Scheduler& scheduler){
  AsyncResult<int> ready;ready.resolve(42);
  if(withTimeout(ready,0,"timeout",scheduler).wait()!=42)return false;
  AsyncResult<int> pending;
  bool timed=false;
  try{withTimeout(pending,0,"timeout",scheduler).wait();}
  catch(const std::runtime_error& e){timed=std::string(e.what())=="timeout";}
  return timed&&scheduler.cancels==2;
}
}
int main(){
  ImmediateScheduler inlineScheduler;OverlapScheduler overlapScheduler;
  if(!check(inlineScheduler)||!check(overlapScheduler)){
    std::cerr<<"schedule callback initialization race failed\n";return 1;
  }
  RealTimerScheduler real;
  for(int i=0;i<500;i++){
    AsyncResult<int> ready;ready.resolve(i);
    if(withTimeout(ready,0,"timeout",real).wait()!=i){
      std::cerr<<"real timer race failed\n";return 1;
    }
  }
  std::cout<<"PASS immediate, overlap and real timer races\n";
}
