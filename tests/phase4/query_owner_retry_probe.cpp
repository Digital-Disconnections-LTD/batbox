#include "query/OwnerRetry.hpp"
#include <future>
#include <thread>

static batbox::Json ownerRetryBridge(const batbox::retry::RetryOptions& options,
    batbox::retry::Dependencies& deps) {
  batbox::event_loop::Loop loop;
  std::promise<batbox::Json> settled;
  auto future=settled.get_future();
  std::thread worker([&]{
    try {settled.set_value(batbox::query::withRetryOwnerSegments(loop,options,deps));}
    catch (...) {settled.set_exception(std::current_exception());}
    loop.postMission([&]{loop.stop();});
  });
  auto hold=loop.hold();
  loop.run();
  hold.release();
  worker.join();
  return future.get();
}

#define withRetry ownerRetryBridge
#include "../phase2/retry_probe.cpp"
#undef withRetry
