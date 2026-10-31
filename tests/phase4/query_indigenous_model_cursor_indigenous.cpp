#include "query/IndigenousModelCursor.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>

using namespace batbox;
namespace {
std::optional<model::MessageRef> await(event_loop::Loop& loop,
    event_loop::Deferred<std::optional<model::MessageRef>> pending){
  std::optional<std::optional<model::MessageRef>> result;
  std::exception_ptr error;
  pending.then([&](const std::optional<model::MessageRef>* value,
      const std::exception_ptr& rejected){
    error=rejected;if(value)result=*value;loop.stop();});
  auto hold=loop.hold();loop.run();hold.release();
  if(error)std::rethrow_exception(error);
  if(!result)throw std::logic_error("Native model cursor did not settle");
  return *result;
}
}
int main(){
  event_loop::Loop loop;
  std::atomic_int resumed=0;
  auto message=std::make_shared<Json>(Json{{"type","assistant"},
    {"message",{{"usage",{{"input_tokens",26}}}}}});
  auto cursor=query::makeProducerModelCursor(loop,
    [&](const std::function<void(const model::MessageRef&)>& yield,
        std::stop_token){
      yield(message);
      ++resumed;
      (*message)["message"]["usage"]["input_tokens"]=42;
      yield(std::make_shared<Json>(Json{{"type","stream_event"}}));
      ++resumed;
    });
  const auto first=await(loop,cursor.next());
  assert(first&&*first==message&&resumed.load()==0);
  assert((**first)["message"]["usage"]["input_tokens"]==26);
  const auto second=await(loop,cursor.next());
  assert(second&&(**second)["type"]=="stream_event");
  assert(resumed.load()==1&&(**first)["message"]["usage"]["input_tokens"]==42);
  const auto end=await(loop,cursor.next());
  assert(!end&&resumed.load()==2);
  assert(!await(loop,cursor.next()));

  auto failing=query::makeProducerModelCursor(loop,
    [](const std::function<void(const model::MessageRef&)>& yield,std::stop_token){
      yield(std::make_shared<Json>(Json{{"type","partial"}}));
      throw std::runtime_error("stream failure");
    });
  assert(await(loop,failing.next()));
  bool rejected=false;
  try{(void)await(loop,failing.next());}
  catch(const std::runtime_error& error){rejected=std::string(error.what())=="stream failure";}
  assert(rejected&&!await(loop,failing.next()));

  std::stop_source userStop;
  auto cancelled=query::makeProducerModelCursor(loop,
    [](const std::function<void(const model::MessageRef&)>&,
        std::stop_token stop){
      if(!stop.stop_requested())throw std::logic_error("stop was not forwarded");
    },userStop.get_token());
  userStop.request_stop();
  assert(!await(loop,cancelled.next()));
  // The producer can be waiting for an owner callback when the consumer
  // abandons its cursor. Releasing the cursor must not join on the owner.
  auto gate=std::make_shared<std::promise<void>>();
  auto gateReady=gate->get_future().share();
  std::atomic_bool posted=false,workerExited=false;
  auto held=query::makeProducerModelCursor(loop,
    [&,gateReady](const std::function<void(const model::MessageRef&)>&,
        std::stop_token){
      loop.dispatcher().postMission([gate]{gate->set_value();});
      posted=true;
      gateReady.wait();
      workerExited=true;
    });
  (void)held.next();
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
  while(!posted&&std::chrono::steady_clock::now()<deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(posted);
  held.next={};
  loop.runReady();
  while(!workerExited&&std::chrono::steady_clock::now()<deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(workerExited);
  std::atomic_bool cleanup=false;
  auto closeable=query::makeProducerModelCursor(loop,
    [&](const auto& yield,std::stop_token){
      yield(std::make_shared<Json>(Json{{"type","assistant"}}));
      cleanup=true;
    });
  assert(await(loop,closeable.next()));
  assert(closeable.close);
  bool closed=false;
  closeable.close().then([&](const event_loop::Unit*,const std::exception_ptr& error){
    assert(!error);closed=true;loop.stop();
  });
  auto closeHold=loop.hold();loop.run();closeHold.release();
  assert(closed&&cleanup);
  std::cout<<"native model cursor backpressure, late ref mutation, error and stop passed\n";
}
