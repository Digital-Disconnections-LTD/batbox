#include "query/CloseableIndigenousCursor.hpp"
#include "query/IndigenousModelCursor.hpp"
#include <cassert>
#include <future>
#include <string>
#include <vector>

using namespace batbox;
namespace {
template<class T>T await(event_loop::Loop& loop,event_loop::Deferred<T> pending){
  std::optional<T> value;std::exception_ptr error;
  pending.then([&](const T* item,const std::exception_ptr& rejected){
    error=rejected;if(item)value=*item;loop.stop();
  });
  auto hold=loop.hold();loop.run();hold.release();
  if(error)std::rethrow_exception(error);
  if(!value)throw std::logic_error("closeable cursor did not settle");
  return *value;
}
}
int main(){
  event_loop::Loop loop;
  std::vector<std::string> trace;
  auto unstarted=query::makeCloseableIndigenousCursor<model::MessageRef>(loop,
    [&](const auto&,std::stop_token){trace.push_back("unexpected_start");});
  (void)await(loop,unstarted.close());
  assert(trace.empty());
  auto cursor=query::makeCloseableIndigenousCursor<model::MessageRef>(loop,
    [&](const auto& yield,std::stop_token stop){
      trace.push_back("producer_start");
      yield(std::make_shared<Json>(Json{{"type","assistant"}}));
      trace.push_back("producer_resumed");
      if(stop.stop_requested())trace.push_back("producer_finally");
      else throw std::logic_error("close did not stop producer");
    });
  const auto first=await(loop,cursor.next());
  assert(first&&(**first)["type"]=="assistant");
  assert(trace==std::vector<std::string>({"producer_start"}));
  (void)await(loop,cursor.close());
  trace.push_back("prefetch_dispose");
  assert(trace==std::vector<std::string>({"producer_start",
    "producer_resumed","producer_finally","prefetch_dispose"}));
  (void)await(loop,cursor.close());

  auto throwing=query::makeCloseableIndigenousCursor<model::MessageRef>(loop,
    [&](const auto& yield,std::stop_token){
      yield(std::make_shared<Json>(Json{{"type","assistant"}}));
      throw std::runtime_error("cleanup failed");
    });
  assert(await(loop,throwing.next()));
  bool propagated=false;
  try{(void)await(loop,throwing.close());}
  catch(const std::runtime_error& error){
    propagated=std::string(error.what())=="cleanup failed";
  }
  assert(propagated);

  std::vector<std::string> ownerTrace;
  auto blocked=query::makeCloseableIndigenousCursor<model::MessageRef>(loop,
    [&](const auto& yield,std::stop_token){
      yield(std::make_shared<Json>(Json{{"type","assistant"}}));
      std::promise<void> gate;auto ready=gate.get_future();
      loop.dispatcher().postMission([&]{ownerTrace.push_back("owner_callback");
        gate.set_value();});
      ready.wait();
      ownerTrace.push_back("producer_finally");
    });
  assert(await(loop,blocked.next()));
  (void)await(loop,blocked.close());
  ownerTrace.push_back("prefetch_dispose");
  assert(ownerTrace==std::vector<std::string>({"owner_callback",
    "producer_finally","prefetch_dispose"}));
}
