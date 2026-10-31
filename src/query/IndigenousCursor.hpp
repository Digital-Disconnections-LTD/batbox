#pragma once
#include "runtime/AsyncLaunch.hpp"
#include "event_loop/Deferred.hpp"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>

namespace batbox::query {
template<class T>using IndigenousProducer=std::function<void(
  const std::function<void(const T&)>& yield,std::stop_token stop)>;

namespace indigenous_cursor {
template<class T>struct State {
  explicit State(event_loop::Loop& loop):dispatcher(loop.dispatcher()){}
  event_loop::Loop::Dispatcher dispatcher;
  std::mutex mutex;
  std::condition_variable changed;
  bool pausedAtYield=false;
  bool workerYieldDelivered=false;
  bool closed=false;
  bool done=false;
  bool errorDelivered=false;
  std::exception_ptr error;
  struct Queued {T value;bool fromWorker;};
  std::deque<Queued> queued;
  std::optional<event_loop::Deferred<std::optional<T>>> pending;
};

template<class T>void deliverQueued(const std::shared_ptr<State<T>>& state){
  std::optional<event_loop::Deferred<std::optional<T>>> pending;
  std::optional<typename State<T>::Queued> item;
  {
    std::lock_guard lock(state->mutex);
    if(state->closed||!state->pending||state->queued.empty())return;
    pending=std::move(state->pending);state->pending.reset();
    item=std::move(state->queued.front());state->queued.pop_front();
    if(item->fromWorker)state->workerYieldDelivered=true;
  }
  pending->resolve(std::move(item->value));
}

template<class T>class ProducerCursor {
public:
  ProducerCursor(event_loop::Loop& loop,IndigenousProducer<T> producer,
      std::stop_token external):state_(std::make_shared<State<T>>(loop)),
      producer_(std::move(producer)){
    if(!producer_)throw std::invalid_argument("Native producer is required");
    if(external.stop_possible())externalStop_.emplace(external,
      std::function<void()>([this]{cancel_.request_stop();
        std::lock_guard lock(state_->mutex);state_->changed.notify_all();}));
  }
  ~ProducerCursor(){
    cancel_.request_stop();
    {
      std::lock_guard lock(state_->mutex);
      state_->closed=true;state_->pausedAtYield=false;
    }
    state_->changed.notify_all();
    if(worker_.joinable()){
      bool completed=false;
      {
        std::lock_guard lock(state_->mutex);
        completed=state_->done;
      }
      if(state_->dispatcher.isOwnerThread()&&!completed){
        // A producer may be awaiting an owner-dispatched imported callback.
        // Joining here would block that callback and deadlock the loop.
        std::shared_ptr<std::thread> reaping;
        try{
          reaping=std::make_shared<std::thread>(std::move(worker_));
          ::batbox::runtime::contextThread([reaping]{reaping->join();}).detach();
        }catch(...){
          // Resource exhaustion must not terminate from this destructor.
          // The worker retains all of its own captured state until exit.
          try{
            if(reaping&&reaping->joinable())reaping->detach();
            else if(worker_.joinable())worker_.detach();
          }catch(...){
            if(reaping&&reaping->joinable())reaping->join();
            else if(worker_.joinable())worker_.join();
          }
        }
      }else worker_.join();
    }
  }
  event_loop::Deferred<std::optional<T>> next(){
    if(!state_->dispatcher.isOwnerThread())
      throw std::logic_error("Native cursor next() requires loop owner");
    event_loop::Deferred<std::optional<T>> response(state_->dispatcher);
    bool start=false;
    {
      std::lock_guard lock(state_->mutex);
      if(state_->pending)throw std::logic_error("Concurrent native cursor next()");
      if(state_->workerYieldDelivered){
        state_->workerYieldDelivered=false;
        state_->pausedAtYield=false;
        state_->changed.notify_all();
      }
      if(!state_->queued.empty()){
        auto item=std::move(state_->queued.front());state_->queued.pop_front();
        if(item.fromWorker)state_->workerYieldDelivered=true;
        response.resolve(std::move(item.value));
        return response;
      }
      if(state_->done){
        if(state_->error&&!state_->errorDelivered){
          state_->errorDelivered=true;response.reject(state_->error);
        }else response.resolve(std::nullopt);
        return response;
      }
      state_->pending=response;
      if(!started_){started_=true;start=true;}
    }
    if(start){
      auto state=state_;
      auto producer=producer_;
      const auto stop=cancel_.get_token();
      worker_=::batbox::runtime::contextThread([state,producer=std::move(producer),stop]() mutable {
        auto emit=[state,stop](const T& value){
          const bool owner=state->dispatcher.isOwnerThread();
          {
            std::lock_guard lock(state->mutex);
            if(state->closed||stop.stop_requested())return;
            state->queued.push_back({value,!owner});
            if(!owner)state->pausedAtYield=true;
          }
          // Gadget progress may originate on the loop owner while the worker
          // awaits a gadget Deferred. Waiting for the next pull there would
          // deadlock the UI; only worker emissions apply pull backpressure.
          if(owner){deliverQueued(state);return;}
          state->dispatcher.postMission([state]{deliverQueued(state);});
          std::unique_lock lock(state->mutex);
          state->changed.wait(lock,[&]{
            return !state->pausedAtYield||state->closed||stop.stop_requested();
          });
        };
        std::exception_ptr failure;
        try{producer(emit,stop);}catch(...){failure=std::current_exception();}
        state->dispatcher.postMission([state,failure]{
          std::optional<event_loop::Deferred<std::optional<T>>> pending;
          std::optional<typename State<T>::Queued> item;
          {
            std::lock_guard lock(state->mutex);
            if(state->closed)return;
            state->done=true;state->error=failure;
            if(state->pending){
              pending=std::move(state->pending);state->pending.reset();
              if(!state->queued.empty()){
                item=std::move(state->queued.front());state->queued.pop_front();
                if(item->fromWorker)state->workerYieldDelivered=true;
              }else if(failure)state->errorDelivered=true;
            }
          }
          if(pending){
            if(item)pending->resolve(std::move(item->value));
            else if(failure)pending->reject(failure);
            else pending->resolve(std::nullopt);
          }
        });
      });
    }
    return response;
  }
private:
  std::shared_ptr<State<T>> state_;
  IndigenousProducer<T> producer_;
  std::stop_source cancel_;
  std::optional<std::stop_callback<std::function<void()>>> externalStop_;
  std::thread worker_;
  bool started_=false;
};
} // namespace indigenous_cursor

template<class T>std::function<event_loop::Deferred<std::optional<T>>()> makeIndigenousPullCursor(
    event_loop::Loop& loop,IndigenousProducer<T> producer,std::stop_token externalStop={}){
  if(!loop.isOwnerThread())throw std::logic_error("Native cursor requires loop owner");
  auto cursor=std::make_shared<indigenous_cursor::ProducerCursor<T>>(
    loop,std::move(producer),externalStop);
  return [cursor]{return cursor->next();};
}
} // namespace batbox::query
