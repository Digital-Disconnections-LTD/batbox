#include "query/OwnerWatchdogTimer.hpp"
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

int main(){
  batbox::event_loop::Loop loop;
  auto hold=loop.hold();
  std::mutex mutex;std::condition_variable ready;bool aborted=false;
  std::vector<std::string> trace;
  batbox::model::WatchdogSettings settings{true,12,6};
  batbox::model::StreamWatchdog watchdog(settings,[&]{
    assert(loop.isOwnerThread());trace.push_back("abort");
    {std::lock_guard lock(mutex);aborted=true;}ready.notify_all();
  },[&]{assert(loop.isOwnerThread());trace.push_back("warn");},
    [&]{assert(loop.isOwnerThread());trace.push_back("timeout");},
    batbox::query::makeOwnerWatchdogTimer(loop));
  std::thread worker([&]{
    watchdog.reset();
    {std::unique_lock lock(mutex);
      if(!ready.wait_for(lock,std::chrono::seconds(2),[&]{return aborted;}))
        std::terminate();}
    watchdog.clear();hold.release();
  });
  loop.run();worker.join();
  assert(watchdog.aborted());
  assert((trace==std::vector<std::string>{"warn","timeout","abort"}));
  std::cout<<"owner timer warning, timeout, abort and cross-thread clear match\n";
}
