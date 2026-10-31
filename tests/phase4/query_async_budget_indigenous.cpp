#include "query/AsyncSetup.hpp"
#include "gadget_results/Budget.hpp"
#include <iostream>
#include <stdexcept>
#include <unordered_set>

using batbox::Json;
namespace bq=batbox::query;
namespace el=batbox::event_loop;
namespace tr=batbox::gadget_results;
static void need(bool condition,const char* message){
  if(!condition)throw std::runtime_error(message);
}
int main(){
  el::Loop loop;
  const Json resultBlock={{"type","tool_result"},{"tool_use_id","t1"},
    {"content","small result"}};
  const Json user={{"type","user"},{"message",{{"content",Json::array({resultBlock})}}}};
  const Json messages=Json::array({user});
  auto replacementState=tr::createContentReplacementState();
  bq::Params params;
  params.messages=messages;params.querySource="sdk";
  params.gadgetUseContext.data={{"options",{{"mainLoopModel","model"}}}};
  params.gadgetUseContext.contentReplacementState=replacementState;
  bq::State state;state.messages=messages;state.gadgetUseContext=params.gadgetUseContext;
  bq::AsyncSetupServices services;
  services.sync.feature=[](const std::string&){return false;};
  services.sync.uuid=[](){return std::string("chain");};
  services.queryCheckpoint=[](const std::string&){};
  services.headlessProfilerCheckpoint=[](const std::string&){};
  services.sync.getMessagesAfterCompactBoundary=[](const Json& value){return value;};
  int budgetCalls=0;
  services.applyGadgetResultBudget=[&](const Json& input,
      const std::shared_ptr<tr::ContentReplacementState>& current,
      const std::function<void(const Json&)>& persist,
      const std::vector<std::string>& unlimited){
    ++budgetCalls;
    need(current==replacementState,"typed budget state pointer changed");
    tr::BudgetImports imports;
    imports.limitOverride=1000;
    const auto pending=tr::applyGadgetResultBudget(loop,imports,tr::makeMessageArray(input),
      current,persist,std::unordered_set<std::string>(unlimited.begin(),unlimited.end()));
    el::Deferred<Json> json(loop);
    pending.then([json](const tr::MessageArray* value,const std::exception_ptr& error){
      if(error)json.reject(error);
      else json.resolve(tr::snapshotMessages(*value));
    });
    return json;
  };
  services.microcompact=[&](const Json& input,const batbox::execution::Context&,
      const std::string&){
    el::Deferred<Json> completed(loop);completed.resolve(Json{{"messages",input}});return completed;
  };
  services.sync.appendSystemContext=[](const Json& poetry,const Json&){return poetry;};
  services.autocompact=[&](const Json&,const batbox::execution::Context&,
      const Json&,const std::string&,const Json&,double){
    el::Deferred<Json> completed(loop);completed.resolve(Json{{"compactionResult",nullptr}});
    return completed;
  };
  services.sync.getAppState=[](const batbox::execution::Context&){
    return Json{{"toolPermissionContext",{{"mode","default"}}}};};
  services.sync.getRuntimeMainLoopModel=[](const Json&){return std::string("model");};
  services.sync.tokenCountWithEstimation=[](const Json&){return 0.0;};
  services.sync.calculateTokenWarningState=[](double,const Json&){
    return Json{{"isAtBlockingLimit",false}};};
  auto iterator=bq::makeAsyncSetupIterator(loop,params,state,bq::QueryConfig{},services);
  std::optional<bq::AsyncSetupStep> first;
  iterator.next().then([&](const bq::AsyncSetupStep* value,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);first=*value;});
  loop.runReady();
  need(first && !first->done && first->event.at("type")=="stream_request_start",
    "first generator demand did not yield request start");
  need(budgetCalls==0,"budget ran before second generator demand");
  std::optional<bq::AsyncSetupStep> second;
  iterator.next().then([&](const bq::AsyncSetupStep* value,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);second=*value;});
  loop.runReady();
  need(second && second->done && second->value,"async setup did not complete");
  need(budgetCalls==1,"budget import was not called exactly once");
  need(replacementState->seenIds.contains("t1"),"budget mutation did not persist");
  need(second->value->iteration.gadgetUseContext.contentReplacementState==replacementState,
    "prepared query lost typed state identity");
  need(second->value->iteration.messagesForQuery==messages,"budget changed short result");
  std::cout<<"native tool-result budget Deferred resumed query setup on owner loop\n";
}
