#include "bats/BatGadgetStream.hpp"
#include "runtime/MessageSnapshot.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace std::chrono_literals;
int main(){
  using namespace batbox;
  auto ref=std::make_shared<Json>(Json{{"type","assistant"},{"message",{{"usage",0}}}});
  bats::RunBatParams params;
  bats::BatGadgetStream stream(params,[ref](const bats::RunBatParams& request,
      const query::Yield& yield){
    assert(request.overrideAbortOwner);
    yield(ref);
    {
      runtime::MessageLock guard(ref);
      guard.value()["message"]["usage"]=42;
    }
    yield(Json{{"type","stream_event"}});
  });
  const auto first=stream.next().get();
  assert(first && *first==ref);
  std::this_thread::sleep_for(20ms);
  assert(runtime::snapshotMessage(ref).at("message").at("usage")==0);
  const auto second=stream.next().get();
  assert(second && runtime::snapshotMessage(ref).at("message").at("usage")==42);
  assert(stream.next().get()==std::nullopt);
  assert(stream.returnWithin(100ms));

  bats::BatGadgetStream errorStream(params,[](const bats::RunBatParams&,
      const query::Yield&){throw std::runtime_error("stream error");});
  bool saw=false;
  try{errorStream.next().get();}catch(const std::runtime_error& error){saw=std::string(error.what())=="stream error";}
  assert(saw);
  assert(errorStream.returnWithin(100ms));

  bats::BatGadgetStream queued(params,[](const bats::RunBatParams&,
      const query::Yield& yield){
    yield(Json{{"type","first"}});
    yield(Json{{"type","second"}});
  });
  const auto queuedFirst=queued.next();
  const auto queuedSecond=queued.next();
  const auto queuedDone=queued.next();
  assert(queuedFirst.get().value()->at("type")=="first");
  assert(queuedSecond.get().value()->at("type")=="second");
  assert(!queuedDone.get().has_value());
  assert(queued.returnWithin(100ms));

  bats::BatGadgetStream cancelled(params,[](const bats::RunBatParams&,
      const query::Yield& yield){yield(Json{{"type","assistant"}});
    throw std::runtime_error("must not resume after return");});
  assert(cancelled.next().get().has_value());
  assert(cancelled.returnWithin(100ms));
  std::cout<<"BatGadget demand-driven iterator, late ref mutation and return passed\n";
}
