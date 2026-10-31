#include "query/IndigenousModelCursor.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

using namespace batbox;
namespace {
void waitFlag(const std::atomic_bool& flag){
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
  while(!flag&&std::chrono::steady_clock::now()<deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(flag);
}
}
int main(){
  event_loop::Loop loop;
  std::stop_source userStop;
  std::atomic_bool entered=false,left=false;
  auto cursor=query::makeProducerModelCursor(loop,
    [&](const std::function<void(const model::MessageRef&)>&,
        std::stop_token stop){
      entered=true;
      std::mutex mutex;
      std::condition_variable_any changed;
      std::unique_lock lock(mutex);
      changed.wait(lock,stop,[]{return false;});
      left=true;
    },userStop.get_token());
  auto pending=cursor.next();
  waitFlag(entered);
  bool settled=false;
  pending.then([&](const std::optional<model::MessageRef>* value,
      const std::exception_ptr& error){
    assert(!error&&value&&!*value);settled=true;loop.stop();
  });
  auto hold=loop.hold();
  loop.setTimeout(10,[&]{userStop.request_stop();});
  loop.run();hold.release();
  assert(settled);
  waitFlag(left);

  // Abandoning an unobserved pending pull cancels its worker without joining
  // on the owner. The Deferred itself stays pending, like a discarded Promise.
  std::atomic_bool abandonedEntered=false,abandonedLeft=false;
  auto abandoned=query::makeProducerModelCursor(loop,
    [&](const std::function<void(const model::MessageRef&)>&,
        std::stop_token stop){
      abandonedEntered=true;
      std::mutex mutex;
      std::condition_variable_any changed;
      std::unique_lock lock(mutex);
      changed.wait(lock,stop,[]{return false;});
      abandonedLeft=true;
    });
  (void)abandoned.next();
  waitFlag(abandonedEntered);
  abandoned={};
  waitFlag(abandonedLeft);
}
