#include "query/OwnerRetry.hpp"
#include "retry/Retry.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <future>
#include <thread>

using namespace std::chrono_literals;

int main() {
  batbox::event_loop::Loop loop;
  batbox::retry::RetryOptions options;
  options.model="local-model";
  options.thinkingConfig={{"type","disabled"}};
  options.maxRetries=1;
  batbox::retry::Dependencies deps;
  deps.wire.retryAfter="retry-after";
  deps.random=[]{return 0.25;};
  deps.nowMs=[]{return 1000.0;};
  deps.isFastModeEnabled=[&]{assert(loop.isOwnerThread());return false;};
  deps.isSubscriber=[&]{assert(loop.isOwnerThread());return false;};
  deps.isDefaultPremiumModel=[&](const std::string&){
    assert(loop.isOwnerThread());return false;};
  deps.getClient=[&]{assert(!loop.isOwnerThread());return batbox::Json::object();};
  std::atomic<int> heartbeats=0;
  int attempts=0,notices=0;
  bool microtaskRan=false;
  deps.operation=[&](const batbox::Json&,double,const batbox::retry::RetryContext&){
    assert(!loop.isOwnerThread());
    if(++attempts==1){
      loop.postMission([&]{loop.setTimeout(5,[&]{heartbeats.fetch_add(1);},true);});
      const auto deadline=std::chrono::steady_clock::now()+500ms;
      while(heartbeats.load()==0 && std::chrono::steady_clock::now()<deadline)
        std::this_thread::sleep_for(1ms);
      assert(heartbeats.load()>0);
      batbox::retry::ErrorRecord error;
      error.kind=batbox::retry::ErrorRecord::Kind::Api;
      error.status=500;
      error.message="server error";
      error.headers={{"retry-after","0"}};
      throw batbox::retry::RetryFailure(error);
    }
    return batbox::Json{{"ok",true}};
  };
  deps.logForDebugging=[&](const std::string&){
    assert(loop.isOwnerThread());
    loop.queueMicrotask([&]{microtaskRan=true;});
  };
  deps.createSystemAPIErrorMessage=[&](const batbox::retry::ErrorRecord&,
      double,double,double){
    assert(loop.isOwnerThread());
    assert(!microtaskRan); // No microtask between synchronous statements.
    return batbox::Json{{"type","system"}};
  };
  deps.yield=[&](const batbox::Json&){assert(!loop.isOwnerThread());++notices;};
  std::promise<batbox::Json> settled;
  auto future=settled.get_future();
  std::thread worker([&]{
    try{settled.set_value(batbox::query::withRetryOwnerSegments(loop,options,deps));}
    catch(...){settled.set_exception(std::current_exception());}
    loop.postMission([&]{loop.stop();});
  });
  auto hold=loop.hold();
  loop.run();
  hold.release();
  worker.join();
  assert((future.get()==batbox::Json{{"ok",true}}));
  assert(attempts==2 && notices==1 && heartbeats.load()>0);
  assert(microtaskRan);
}
