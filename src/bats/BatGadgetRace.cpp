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
} // namespace batbox::bats
