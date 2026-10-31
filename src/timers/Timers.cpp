#include "runtime/AsyncLaunch.hpp"
#include "timers/Timers.hpp"
#include <cmath>
#include <cstdint>
#include <thread>

namespace batbox::timers {
namespace {
struct TimerState {std::mutex mutex;std::condition_variable cv;bool cancelled=false;
  std::function<void()> callback;};
class RealHandle final:public TimerHandle {
public:
  explicit RealHandle(std::shared_ptr<TimerState> state):state_(std::move(state)){}
  void cancel()override{
    std::lock_guard lock(state_->mutex);
    state_->cancelled=true;state_->callback={};state_->cv.notify_all();
  }
private:std::shared_ptr<TimerState> state_;
};
long timeoutMs(double value){
  if(!std::isfinite(value)||value<1||value>2147483647)return 1;
  return static_cast<long>(value);
}
struct SleepState {
  SleepResult result;
  std::shared_ptr<TimerHandle> timer;
  std::unique_ptr<std::stop_callback<std::function<void()>>> abortCallback;
  SleepOptions options;
  std::mutex mutex;
  bool done=false;
  bool markDone(){std::lock_guard lock(mutex);if(done)return false;done=true;return true;}
};
void abortSleep(const std::shared_ptr<SleepState>& state){
  if(!state->markDone())return;
  if(state->timer)state->timer->cancel();
  if(state->options.throwOnAbort||state->options.abortError){
    if(state->options.abortError){
      try{state->result.reject(state->options.abortError());}
      catch(...){state->result.reject(std::current_exception());}
    }else state->result.reject(std::make_exception_ptr(std::runtime_error("aborted")));
  }else state->result.resolve({});
}
}
std::shared_ptr<TimerHandle> RealTimerScheduler::schedule(double ms,std::function<void()> callback,bool /*unref*/){
  auto state=std::make_shared<TimerState>();state->callback=std::move(callback);
  ::batbox::runtime::contextThread([state,delay=timeoutMs(ms)]{
    std::unique_lock lock(state->mutex);
    if(state->cv.wait_for(lock,std::chrono::milliseconds(delay),[&]{return state->cancelled;}))return;
    auto callback=std::move(state->callback);state->callback={};lock.unlock();
    if(callback)callback();
  }).detach();
  return std::make_shared<RealHandle>(state);
}
SleepResult sleep(double ms,std::stop_token signal,const SleepOptions& options,TimerScheduler& scheduler){
  auto state=std::make_shared<SleepState>();state->options=options;
  if(signal.stop_requested()){abortSleep(state);return state->result;}
  state->timer=scheduler.schedule(ms,[state]{
    if(!state->markDone())return;
    state->result.resolve({});
  },options.unref);
  std::weak_ptr<SleepState> weak=state;
  state->abortCallback=std::make_unique<std::stop_callback<std::function<void()>>>(signal,[weak]{
    if(auto state=weak.lock())abortSleep(state);
  });
  return state->result;
}
} // namespace batbox::timers
