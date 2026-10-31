#include "query/AsyncOperationQueue.hpp"
#include <cassert>
#include <string>
#include <vector>

using namespace batbox;
namespace {
template<class T>event_loop::Deferred<T> ready(event_loop::Loop& loop,T value){
  event_loop::Deferred<T> result(loop);result.resolve(std::move(value));return result;
}
query::AsyncEngineStep await(event_loop::Loop& loop,
    event_loop::Deferred<query::AsyncEngineStep> pending){
  std::optional<query::AsyncEngineStep> result;std::exception_ptr error;
  pending.then([&](const query::AsyncEngineStep* value,
      const std::exception_ptr& rejected){
    error=rejected;if(value)result=*value;
  });
  loop.runReady();
  if(error)std::rethrow_exception(error);
  if(!result)throw std::logic_error("generator operation did not settle");
  return *result;
}
}
int main(){
  event_loop::Loop loop;
  std::vector<std::string> trace;
  event_loop::Deferred<query::AsyncEngineStep> model(loop);
  int pulls=0;
  auto queue=std::make_shared<query::AsyncOperationQueue>(loop,
    [&]{
      trace.push_back("next");
      if(++pulls==1)return ready(loop,query::AsyncEngineStep{
        false,std::make_shared<Json>(Json{{"type","stream_request_start"}}),{}});
      return model;
    },
    [&](const Json& value){
      trace.push_back("return");
      return ready(loop,query::AsyncEngineStep{true,{},value});
    },
    [&](std::exception_ptr){
      trace.push_back("throw");
      return ready(loop,query::AsyncEngineStep{false,
        std::make_shared<Json>(Json{{"type","assistant"},{"apiError","error"}}),{}});
    });
  const auto first=await(loop,queue->next());
  assert(!first.done&&(*first.event)["type"]=="stream_request_start");
  auto second=queue->next();
  auto closing=queue->returnEarly("closed");
  bool secondSettled=false,closeSettled=false;
  second.then([&](const query::AsyncEngineStep*,const std::exception_ptr&){
    secondSettled=true;trace.push_back("second_settled");
  });
  closing.then([&](const query::AsyncEngineStep*,const std::exception_ptr&){
    closeSettled=true;trace.push_back("close_settled");
  });
  loop.runReady();
  assert(!secondSettled&&!closeSettled&&trace==std::vector<std::string>({"next","next"}));
  model.resolve(query::AsyncEngineStep{false,
    std::make_shared<Json>(Json{{"type","assistant"}}),{}});
  loop.runReady();
  assert(secondSettled&&closeSettled);
  assert(trace==std::vector<std::string>({"next","next",
    "second_settled","return","close_settled"}));
  const auto closed=await(loop,closing);
  assert(closed.done&&closed.terminal==Json("closed"));
  const auto after=await(loop,queue->next());
  assert(after.done&&!after.terminal);
  bool rejected=false;
  auto thrown=queue->throwEarly(std::make_exception_ptr(std::runtime_error("caller throw")));
  thrown.then([&](const query::AsyncEngineStep*,const std::exception_ptr& error){
    rejected=!!error;
  });
  loop.runReady();assert(rejected);

  event_loop::Deferred<query::AsyncEngineStep> secondModel(loop);
  int call=0,handledThrow=0;
  auto catching=std::make_shared<query::AsyncOperationQueue>(loop,
    [&]{
      if(++call==1)return ready(loop,query::AsyncEngineStep{false,
        std::make_shared<Json>(Json{{"type","stream_request_start"}}),{}});
      if(call==2)return secondModel;
      return ready(loop,query::AsyncEngineStep{true,{},
        Json{{"reason","model_error"}}});
    },
    [&](const Json& value){return ready(loop,query::AsyncEngineStep{true,{},value});},
    [&](std::exception_ptr){
      ++handledThrow;
      return ready(loop,query::AsyncEngineStep{false,
        std::make_shared<Json>(Json{{"type","assistant"},
          {"isApiErrorMessage",true}}),{}});
    });
  assert(!await(loop,catching->next()).done);
  auto awaiting=catching->next();
  auto injected=catching->throwEarly(std::make_exception_ptr(
    std::runtime_error("caller throw")));
  loop.runReady();assert(!awaiting.settled()&&!injected.settled());
  secondModel.resolve(query::AsyncEngineStep{false,
    std::make_shared<Json>(Json{{"type","assistant"}}),{}});
  assert(!await(loop,awaiting).done);
  const auto recovered=await(loop,injected);
  assert(!recovered.done&&(*recovered.event)["isApiErrorMessage"]==true);
  assert(handledThrow==1);
  const auto terminal=await(loop,catching->next());
  assert(terminal.done&&terminal.terminal->at("reason")=="model_error");
}
