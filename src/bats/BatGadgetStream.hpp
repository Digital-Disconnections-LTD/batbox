#pragma once
#include "bats/RunBat.hpp"
#include "bats/CompletionSignal.hpp"
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <optional>
#include <thread>

namespace batbox::bats {
struct ObservedBatMessage {
  std::shared_future<std::optional<query::MessageRef>> future;
  std::shared_ptr<CompletionSignal> completion;
};
// Demand-driven next()/return() contract. Each next() grants one demand. The
// producer pauses after yielding until the next demand, so an assistant ref
// cannot receive message_delta while its foreground consumer is still handling
// the yielded value.
class BatGadgetStream {
public:
  using Runner=std::function<void(const RunBatParams&,const query::Yield&)>;
  BatGadgetStream(RunBatParams params,Runner runner,std::stop_token parentStop={});
  BatGadgetStream(const BatGadgetStream&)=delete;
  BatGadgetStream& operator=(const BatGadgetStream&)=delete;
  ~BatGadgetStream();
  ObservedBatMessage nextObserved();
  std::shared_future<std::optional<query::MessageRef>> next();
  bool returnWithin(std::chrono::milliseconds timeout);
private:
  struct State;
  std::shared_ptr<State> state_;
  std::thread worker_;
  std::unique_ptr<std::stop_callback<std::function<void()>>> parentAbort_;
  bool detached_=false;
};
} // namespace batbox::bats
