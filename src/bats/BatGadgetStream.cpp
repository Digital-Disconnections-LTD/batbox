#include "runtime/AsyncLaunch.hpp"
#include "bats/BatGadgetStream.hpp"
#include <deque>
#include <stdexcept>

namespace batbox::bats {
namespace {
struct IteratorReturned{};
}
struct BatGadgetStream::State {
  struct Pending {
    std::promise<std::optional<query::MessageRef>> promise;
    std::shared_ptr<CompletionSignal> completion;
  };
  std::mutex mutex;
  std::condition_variable cv;
  std::shared_ptr<std::stop_source> cancelOwner=std::make_shared<std::stop_source>();
  std::deque<Pending> pending;
  bool done=false,returning=false;
  std::exception_ptr error;
};

BatGadgetStream::BatGadgetStream(RunBatParams params,Runner runner,
    std::stop_token parentStop):state_(std::make_shared<State>()){
  if(!runner)throw std::invalid_argument("BatGadget stream requires runAgent");
  params.overrideAbortOwner=state_->cancelOwner;
  if(parentStop.stop_possible()){
    parentAbort_=std::make_unique<std::stop_callback<std::function<void()>>>(
      parentStop,[owner=state_->cancelOwner]{owner->request_stop();});
  }
  worker_=::batbox::runtime::contextThread([state=state_,params=std::move(params),runner=std::move(runner)]()mutable{
    try{
      {
        std::unique_lock lock(state->mutex);
        state->cv.wait(lock,[&]{return !state->pending.empty() || state->returning;});
        if(state->returning)throw IteratorReturned{};
      }
      runner(params,query::Yield::withReferences([state](const query::MessageRef& ref){
        std::unique_lock lock(state->mutex);
        if(state->returning)throw IteratorReturned{};
        if(state->pending.empty())throw std::logic_error("BatGadget stream yielded without demand");
        state->pending.front().completion->settle();
        state->pending.front().promise.set_value(ref);
        state->pending.pop_front();
        state->cv.notify_all();
        state->cv.wait(lock,[&]{return !state->pending.empty() || state->returning;});
        if(state->returning)throw IteratorReturned{};
      }));
    }catch(const IteratorReturned&){
    }catch(...){
      std::lock_guard lock(state->mutex);
      state->error=std::current_exception();
    }
    {
      std::lock_guard lock(state->mutex);
      state->done=true;
      for(auto& pending:state->pending){
        pending.completion->settle();
        if(state->error && !state->returning)
          pending.promise.set_exception(state->error);
        else pending.promise.set_value(std::nullopt);
      }
      state->pending.clear();
    }
    state->cv.notify_all();
  });
}

BatGadgetStream::~BatGadgetStream(){
  if(worker_.joinable() && !detached_)returnWithin(std::chrono::milliseconds(1000));
}

ObservedBatMessage BatGadgetStream::nextObserved(){
  std::lock_guard lock(state_->mutex);
  State::Pending pending;
  pending.completion=std::make_shared<CompletionSignal>();
  auto future=pending.promise.get_future().share();
  const auto completion=pending.completion;
  if(state_->done){
    completion->settle();
    if(state_->error && !state_->returning)
      pending.promise.set_exception(state_->error);
    else pending.promise.set_value(std::nullopt);
    return {future,completion};
  }
  state_->pending.push_back(std::move(pending));
  state_->cv.notify_all();
  return {future,completion};
}

std::shared_future<std::optional<query::MessageRef>> BatGadgetStream::next(){
  return nextObserved().future;
}

bool BatGadgetStream::returnWithin(std::chrono::milliseconds timeout){
  {
    std::lock_guard lock(state_->mutex);
    state_->returning=true;
    for(auto& pending:state_->pending){
      pending.completion->settle();
      pending.promise.set_value(std::nullopt);
    }
    state_->pending.clear();
  }
  state_->cancelOwner->request_stop();
  state_->cv.notify_all();
  bool done;
  {
    std::unique_lock lock(state_->mutex);
    done=state_->cv.wait_for(lock,timeout,[&]{return state_->done;});
  }
  if(worker_.joinable()){
    if(done)worker_.join();
    else{worker_.detach();detached_=true;}
  }
  return done;
}
} // namespace batbox::bats
