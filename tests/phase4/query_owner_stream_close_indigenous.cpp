#include "query/IndigenousModelCursor.hpp"
#include "query/OwnerStreamConsume.hpp"
#include <atomic>
#include <cassert>
#include <optional>
#include <vector>

using namespace batbox;
namespace {
template<class T>T await(event_loop::Loop& loop,event_loop::Deferred<T> pending){
  std::optional<T> value;std::exception_ptr failure;
  pending.then([&](const T* item,const std::exception_ptr& error){
    if(item)value=*item;failure=error;loop.stop();
  });
  auto hold=loop.hold();loop.run();hold.release();
  if(failure)std::rethrow_exception(failure);
  if(!value)throw std::logic_error("Owner stream did not settle");
  return *value;
}
void scenario(bool stopAfterAssistant){
  event_loop::Loop loop;
  std::atomic_int aborts=0,costCalls=0,errorCalls=0;
  std::atomic_bool generatorClosed=false;
  std::vector<Json> events={
    Json{{"type","message_start"},{"message",{{"type","message"},
      {"content",Json::array()},{"usage",{{"input_tokens",1},
        {"output_tokens",0}}}}}},
    Json{{"type","content_block_start"},{"index",0},
      {"content_block",{{"type","text"},{"text",""}}}},
    Json{{"type","content_block_delta"},{"index",0},
      {"delta",{{"type","text_delta"},{"text","hello"}}}},
    Json{{"type","content_block_stop"},{"index",0}},
    Json{{"type","message_delta"},{"delta",{{"stop_reason","max_tokens"}}},
      {"usage",{{"output_tokens",2}}}}
  };
  for(const Json& event:events)
    if(!event.is_object())throw std::logic_error("Bad event: "+event.dump());
  streaming::ReducerConfig config;
  config.model="batbox-model";config.resolvedModel="batbox-model";
  config.maxOutputTokens=7;
  config.randomUUID=[&]{assert(loop.isOwnerThread());return std::string("uuid");};
  config.timestamp=[&]{assert(loop.isOwnerThread());return std::string("now");};
  config.calculateUSDCost=[&](const std::string&,const Json&){
    assert(loop.isOwnerThread());++costCalls;return 1.0;};
  config.addToTotalFlightCost=[&](double,const Json&,const std::string&){
    assert(loop.isOwnerThread());return 1.0;};
  config.getErrorMessageIfRefusal=[&](const Json&,const std::string&)
      ->std::optional<Json>{
    assert(loop.isOwnerThread());
    return Json{{"type","assistant"},{"kind","refusal"}};};
  config.createAssistantAPIErrorMessage=[&](const std::string&,
      const std::string&,const std::string&){
    assert(loop.isOwnerThread());++errorCalls;
    return Json{{"type","assistant"},{"kind","api_error"}};};
  auto cursor=query::makeProducerModelCursor(loop,
    [&,events](const std::function<void(const model::MessageRef&)>& yield,
        std::stop_token stop){
      model::OpenedStream opened;
      opened.next=[events,index=std::size_t{0}]() mutable -> std::optional<Json>{
        if(index==events.size())return std::nullopt;
        return events[index++];
      };
      opened.abort=[&]{++aborts;};
      streaming::Reducer reducer(config);
      try{query::consumeOwnerStream(loop.dispatcher(),stop,opened,reducer,
        yield,nullptr);}
      catch(const model::GeneratorClosed&){generatorClosed=true;}
    });
  bool selected=false;
  int rawBlockStops=0,rawMessageDeltas=0;
  while(true){
    auto emission=await(loop,cursor.next());
    assert(emission);
    const Json& message=**emission;
    if(!message.is_object())throw std::logic_error(
      "Owner stream yielded non-object: "+message.dump());
    if(message.value("type","")=="stream_event"){
      const auto type=message.at("event").value("type","");
      if(type=="content_block_stop")++rawBlockStops;
      if(type=="message_delta")++rawMessageDeltas;
    }
    if(stopAfterAssistant&&message.value("type","")=="assistant"&&
        !message.contains("kind"))selected=true;
    if(!stopAfterAssistant&&message.value("kind","")=="refusal")selected=true;
    if(selected){(void)await(loop,cursor.close());break;}
  }
  assert(selected&&generatorClosed&&aborts==1);
  if(stopAfterAssistant){
    assert(rawBlockStops==0&&rawMessageDeltas==0&&costCalls==0);
  }else{
    assert(rawBlockStops==1&&rawMessageDeltas==0);
    assert(costCalls==1&&errorCalls==0);
  }
}
}
int main(){scenario(true);scenario(false);}
