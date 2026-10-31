#pragma once
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace batbox::timers {
class TimerHandle {
public:
  virtual ~TimerHandle()=default;
  virtual void cancel()=0;
};
class TimerScheduler {
public:
  virtual ~TimerScheduler()=default;
  // Callback may race with schedule returning. `unref` is forwarded to the
  // host scheduler; indigenous detached threads have different exit semantics.
  virtual std::shared_ptr<TimerHandle> schedule(double ms,std::function<void()> callback,bool unref)=0;
};
class RealTimerScheduler final:public TimerScheduler {
public:
  std::shared_ptr<TimerHandle> schedule(double ms,std::function<void()> callback,bool unref) override;
};

template<class T>class AsyncResult {
public:
  using Observer=std::function<void(const std::optional<T>&,const std::exception_ptr&)>;
  AsyncResult():state_(std::make_shared<State>()){}
  void resolve(T value)const{settle(std::move(value),{});}
  void reject(std::exception_ptr error)const{settle(std::nullopt,std::move(error));}
  void observe(Observer callback)const{
    std::optional<T> value;std::exception_ptr error;
    {std::lock_guard lock(state_->mutex);
      if(!state_->done){state_->observers.push_back(std::move(callback));return;}
      value=state_->value;error=state_->error;
    }
    callback(value,error);
  }
  bool settled()const{std::lock_guard lock(state_->mutex);return state_->done;}
  T wait()const{
    std::unique_lock lock(state_->mutex);
    state_->cv.wait(lock,[&]{return state_->done;});
    auto error=state_->error;auto value=state_->value;lock.unlock();
    if(error)std::rethrow_exception(error);
    return *value;
  }
private:
  struct State {std::mutex mutex;std::condition_variable cv;bool done=false;
    std::optional<T> value;std::exception_ptr error;std::vector<Observer> observers;};
  std::shared_ptr<State> state_;
  void settle(std::optional<T> value,std::exception_ptr error)const{
    std::vector<Observer> observers;
    {std::lock_guard lock(state_->mutex);
      if(state_->done)return;
      state_->done=true;state_->value=std::move(value);state_->error=std::move(error);
      observers=std::move(state_->observers);
    }
    state_->cv.notify_all();
    for(auto& observer:observers)observer(state_->value,state_->error);
  }
};
using SleepResult=AsyncResult<std::monostate>;
struct SleepOptions {
  bool throwOnAbort=false;
  std::function<std::exception_ptr()> abortError;
  bool unref=false;
};
SleepResult sleep(double ms,std::stop_token signal,const SleepOptions& options,TimerScheduler& scheduler);

template<class T>AsyncResult<T> withTimeout(const AsyncResult<T>& promise,double ms,
  const std::string& message,TimerScheduler& scheduler){
  struct Race {
    AsyncResult<T> result;
    std::shared_ptr<TimerHandle> timer;
    std::mutex mutex;
    bool initialized=false;
    bool pendingTimeout=false;
    bool complete=false;
    void settle(const std::optional<T>& value,const std::exception_ptr& error){
      std::shared_ptr<TimerHandle> currentTimer;
      {
        std::lock_guard lock(mutex);
        if(complete)return;
        complete=true;
        currentTimer=timer;
      }
      currentTimer->cancel();
      if(error)result.reject(error);
      else result.resolve(*value);
    }
    void timeout(const std::string& message){
      {
        std::lock_guard lock(mutex);
        if(!initialized){pendingTimeout=true;return;}
      }
      settle(std::nullopt,std::make_exception_ptr(std::runtime_error(message)));
    }
  };
  auto race=std::make_shared<Race>();
  std::weak_ptr<Race> weak=race;
  auto timer=scheduler.schedule(ms,[race,message]{race->timeout(message);},true);
  {std::lock_guard lock(race->mutex);race->timer=std::move(timer);}
  promise.observe([weak](const std::optional<T>& value,const std::exception_ptr& error){
    if(auto race=weak.lock())race->settle(value,error);
  });
  bool pending;
  {std::lock_guard lock(race->mutex);race->initialized=true;pending=race->pendingTimeout;}
  if(pending)race->timeout(message);
  return race->result;
}
} // namespace batbox::timers
