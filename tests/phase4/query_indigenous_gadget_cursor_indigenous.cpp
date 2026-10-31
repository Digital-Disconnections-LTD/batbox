#include "query/IndigenousGadgetCursor.hpp"
#include <atomic>
#include <cassert>
#include <future>
#include <iostream>

using namespace batbox;
namespace {
template<class T>std::future<T> ready(T value){
  std::promise<T> promise;promise.set_value(std::move(value));return promise.get_future();
}
template<class T>std::optional<T> await(event_loop::Loop& loop,
    event_loop::Deferred<std::optional<T>> pending){
  std::optional<std::optional<T>> result;std::exception_ptr error;
  pending.then([&](const std::optional<T>* value,const std::exception_ptr& rejected){
    error=rejected;if(value)result=*value;loop.stop();});
  auto hold=loop.hold();loop.run();hold.release();
  if(error)std::rethrow_exception(error);
  if(!result)throw std::logic_error("Native tool cursor did not settle");
  return *result;
}
void awaitClosed(event_loop::Loop& loop,event_loop::Deferred<event_loop::Unit> pending){
  bool settled=false;std::exception_ptr error;
  pending.then([&](const event_loop::Unit*,const std::exception_ptr& rejected){
    settled=true;error=rejected;loop.stop();});
  auto hold=loop.hold();loop.run();hold.release();
  assert(settled);
  if(error)std::rethrow_exception(error);
}
}
int main(){
  event_loop::Loop loop;
  std::atomic_int completed=0;
  std::atomic_int calls=0;
  gadgets::Gadget read;
  read.name="Read";
  read.parseInput=[](const Json& input){return input;};
  read.isConcurrencySafe=[](const Json&){return false;};
  read.interruptBehavior=[](){return std::string("cancel");};
  read.call=[&](const Json&,const gadgets::GadgetCallContext&,
      const std::function<std::future<Json>(const Json&)>&,
      const Json&,const std::function<void(const gadgets::GadgetProgress&)>&){
    ++calls;
    gadgets::GadgetResult result;result.data={{"text","read done"}};
    return ready(std::move(result));
  };
  read.mapGadgetResultToGadgetResultBlockParam=[](const Json& output,const std::string& id){
    return Json{{"type","tool_result"},{"tool_use_id",id},
      {"content",output.at("text")}};
  };
  execution::Services services;
  services.createUserMessage=[](const Json& args){
    return Json{{"type","user"},{"message",{{"content",args.at("content")}}},
      {"sourceToolAssistantUUID",args.at("sourceToolAssistantUUID")}};};
  services.preGadgetUseHooks=[](const gadgets::Gadget&,const Json&,const Json&,
      const execution::Context&,const std::function<void(const Json&)>&){};
  services.resolvePermissionDecision=[](const Json&,const gadgets::Gadget&,
      const Json& input,const execution::Context&,const Json&,const std::string&){
    return Json{{"decision",{{"behavior","allow"}}},{"input",input}};};
  services.postGadgetUseHooks=[](const gadgets::Gadget&,const Json&,const Json&,
      const execution::Context&,const std::function<void(const Json&)>&){};
  services.processPreMappedGadgetResultBlock=[](const Json& block,const gadgets::Gadget&){return block;};
  services.canUseGadget=[](const Json&){return ready(Json{{"behavior","allow"}});};
  services.addToGadgetDuration=[](double){};
  services.nowMillis=[](){return std::int64_t{100};};
  services.markGadgetUseComplete=[&](const std::string&){++completed;};
  auto prepared=std::make_shared<query::PreparedIteration>();
  prepared->gadgetUseContext.data={{"messages",Json::array()},
    {"agentId",nullptr}};
  prepared->gadgetUseContext.gadgets={&read};
  const Json block={{"type","tool_use"},{"id","t1"},{"name","Read"},
    {"input",Json::object()}};
  const Json assistant={{"type","assistant"},{"uuid","a1"},
    {"message",{{"content",Json::array({block})}}}};
  auto cursor=query::makeIndigenousGadgetUpdatesCursor(loop,{block},Json::array({assistant}),
    prepared,services,"");
  const auto first=await(loop,cursor.next());
  assert(first&&first->message&&(*first->message)["type"]=="user");
  assert((*first->message)["message"]["content"][0]["content"]=="read done");
  assert(first->newContext&&completed.load()==0);
  assert(!await(loop,cursor.next())&&completed.load()==1);

  // An async-generator return at the first gadget-result yield closes
  // runGadgets before it dispatches the next serial gadget block.
  const Json secondBlock={{"type","tool_use"},{"id","t2"},{"name","Read"},
    {"input",Json::object()}};
  const Json twoGadgetAssistant={{"type","assistant"},{"uuid","a2"},
    {"message",{{"content",Json::array({block,secondBlock})}}}};
  auto closeCursor=query::makeIndigenousGadgetUpdatesCursor(loop,{block,secondBlock},
    Json::array({twoGadgetAssistant}),prepared,services,"");
  const int beforeCloseCalls=calls.load();
  assert(await(loop,closeCursor.next()));
  awaitClosed(loop,closeCursor.close());
  assert(calls.load()==beforeCloseCalls+1);

  auto streaming=std::make_shared<query::PreparedIteration>();
  streaming->gadgetUseContext=prepared->gadgetUseContext;
  streaming->streamingGadgetExecutor=std::make_unique<execution::StreamingGadgetExecutor>(
    streaming->gadgetUseContext,services);
  streaming->streamingGadgetExecutor->addGadget(block,assistant);
  auto remaining=query::makeIndigenousRemainingMessagesCursor(loop,streaming);
  const auto streamed=await(loop,remaining.next());
  assert(streamed&&(**streamed)["type"]=="user");
  assert(!await(loop,remaining.next()));
  // The gadget executor waits on a worker while the gadget itself completes on
  // the same long-lived query owner loop. This is the async Bash call shape.
  bool completedOnOwner=false;
  gadgets::Gadget asyncRead=read;
  asyncRead.call=[&](const Json&,const gadgets::GadgetCallContext&,
      const std::function<std::future<Json>(const Json&)>&,
      const Json&,const std::function<void(const gadgets::GadgetProgress&)>&){
    auto promise=std::make_shared<std::promise<gadgets::GadgetResult>>();
    auto future=promise->get_future();
    loop.dispatcher().postMission([promise,&completedOnOwner,&loop]{
      completedOnOwner=loop.isOwnerThread();
      gadgets::GadgetResult result;result.data={{"text","owner done"}};
      promise->set_value(std::move(result));
    });
    return future;
  };
  auto asyncPrepared=std::make_shared<query::PreparedIteration>();
  asyncPrepared->gadgetUseContext=prepared->gadgetUseContext;
  asyncPrepared->gadgetUseContext.gadgets={&asyncRead};
  auto asyncCursor=query::makeIndigenousGadgetUpdatesCursor(loop,{block},
    Json::array({assistant}),asyncPrepared,services,"");
  const auto asynchronous=await(loop,asyncCursor.next());
  assert(completedOnOwner&&asynchronous&&asynchronous->message);
  assert((*asynchronous->message)["message"]["content"][0]["content"]=="owner done");
  assert(!await(loop,asyncCursor.next()));
  std::cout<<"native runTools and StreamingToolExecutor owner-loop cursors passed\n";
}
