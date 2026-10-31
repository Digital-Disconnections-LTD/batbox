#include "bats/BatGadgetRace.hpp"
#include "event_loop/Deferred.hpp"
#include <chrono>
#include <exception>
#include <functional>
#include <stdexcept>

namespace batbox::bats {
BatGadgetRaceResult raceBatMessageAndBackground(
    ObservedBatMessage nextMessage,
    const std::optional<ObservedBackgroundSignal>& backgroundSignal){
  if(!backgroundSignal)return {false,nextMessage.future.get()};
  if(!nextMessage.completion || !backgroundSignal->completion)
    throw std::invalid_argument("BatGadget race requires completion observations");
  auto waiter=std::make_shared<CompletionWaiter>();
  nextMessage.completion->subscribe(waiter,0);
  backgroundSignal->completion->subscribe(waiter,1);
  if(waiter->waitFirst()==0)return {false,nextMessage.future.get()};
  backgroundSignal->future.get();
  return {true,{}};
}

BatGadgetRaceResult raceBatMessageAndBackground(
    std::shared_future<std::optional<query::MessageRef>> nextMessage,
    const std::optional<std::shared_future<void>>& backgroundSignal){
  if(!backgroundSignal)return BatGadgetRaceResult{false,nextMessage.get()};
  event_loop::Loop loop;
  event_loop::Deferred<BatGadgetRaceResult> messageReady(loop),backgroundReady(loop);
  auto raced=event_loop::race(loop,
    std::vector{messageReady,backgroundReady});
  auto keepAlive=loop.hold();
  event_loop::Loop::TimerHandle pollingTimer;
  std::optional<BatGadgetRaceResult> outcome;
  std::exception_ptr error;
  raced.then([&](const BatGadgetRaceResult* value,const std::exception_ptr& rejection){
    if(rejection)error=rejection;
    else outcome=*value;
    pollingTimer.cancel();
    keepAlive.release();
  });
  std::function<void()> poll;
  poll=[&](){
    using namespace std::chrono_literals;
    if(!messageReady.settled() && nextMessage.wait_for(0ms)==std::future_status::ready){
      try{messageReady.resolve(BatGadgetRaceResult{false,nextMessage.get()});}
      catch(...){messageReady.reject(std::current_exception());}
    }
    if(!backgroundReady.settled() &&
        backgroundSignal->wait_for(0ms)==std::future_status::ready){
      try{backgroundSignal->get();backgroundReady.resolve(BatGadgetRaceResult{true,{}});}
      catch(...){backgroundReady.reject(std::current_exception());}
    }
    if(!messageReady.settled() && !backgroundReady.settled())
      pollingTimer=loop.setTimeout(1,poll);
  };
  poll();
  loop.run();
  if(error)std::rethrow_exception(error);
  return *outcome;
}
} // namespace batbox::bats
