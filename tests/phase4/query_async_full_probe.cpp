#include "query/AsyncCompletion.hpp"
#include "query/AsyncEntry.hpp"
#include "query/AsyncEngineInternal.hpp"
#include "query/AsyncGadgetTurn.hpp"
#include "query/IndigenousModelCursor.hpp"
#include "query/IndigenousCursor.hpp"
#include "query/IndigenousGadgetCursor.hpp"
#include "gadget_results/Budget.hpp"
#include "retry/Retry.hpp"
#include <cassert>
#include <future>
#include <iostream>
#include <unordered_set>

using namespace batbox;
namespace {
template<class T>event_loop::Deferred<T> ready(event_loop::Loop& loop,T value){
  event_loop::Deferred<T> out(loop);out.resolve(std::move(value));return out;
}
template<class T>std::future<T> futureReady(T value){
  std::promise<T> p;p.set_value(std::move(value));return p.get_future();
}
query::AsyncEngineStep await(event_loop::Loop& loop,
    event_loop::Deferred<query::AsyncEngineStep> pending){
  std::optional<query::AsyncEngineStep> step;std::exception_ptr error;
  pending.then([&](const query::AsyncEngineStep* value,
      const std::exception_ptr& rejected){
    error=rejected;if(value)step=*value;loop.stop();});
  auto hold=loop.hold();loop.run();hold.release();
  if(error)std::rethrow_exception(error);
  if(!step)throw std::logic_error("Async query step did not settle");
  return *step;
}
Json assistant(Json content,int number){
  return Json{{"type","assistant"},
    {"uuid","assistant-"+std::to_string(number)},
    {"message",{{"id","message-"+std::to_string(number)},
      {"content",std::move(content)}}}};
}
}
int main(int argc,char** argv){
  const std::string scenario=argc<2?"tool":argv[1];
  event_loop::Loop loop;
  std::stop_source stop;
  auto replacementState=gadget_results::createContentReplacementState();
  int budgetCalls=0,gadgetCalls=0,disposed=0;
  gadgets::Gadget read;
  read.name="Read";
  read.parseInput=[](const Json& input){return input;};
  read.isConcurrencySafe=[](const Json&){return false;};
  read.call=[&](const Json&,const gadgets::GadgetCallContext&,
      const std::function<std::future<Json>(const Json&)>&,const Json&,
      const std::function<void(const gadgets::GadgetProgress&)>&){
    ++gadgetCalls;
    gadgets::GadgetResult result;result.data={{"text","read done"}};
    return futureReady(std::move(result));
  };
  read.mapGadgetResultToGadgetResultBlockParam=[](const Json& output,const std::string& id){
    return Json{{"type","tool_result"},{"tool_use_id",id},
      {"content",output.at("text")}};};
  execution::Services executionServices;
  executionServices.createUserMessage=[](const Json& args){
    Json out={{"type","user"},{"message",{{"content",args.at("content")}}}};
    for(const auto& [key,value]:args.items())if(key!="content")out[key]=value;
    return out;
  };
  executionServices.preGadgetUseHooks=[](const gadgets::Gadget&,const Json&,
      const Json&,const execution::Context&,
      const std::function<void(const Json&)>&){};
  executionServices.resolvePermissionDecision=[](const Json&,
      const gadgets::Gadget&,const Json& input,const execution::Context&,
      const Json&,const std::string&){
    return Json{{"decision",{{"behavior","allow"}}},{"input",input}};};
  executionServices.postGadgetUseHooks=[](const gadgets::Gadget&,const Json&,
      const Json&,const execution::Context&,
      const std::function<void(const Json&)>&){};
  executionServices.processPreMappedGadgetResultBlock=[](const Json& block,
      const gadgets::Gadget&){return block;};
  executionServices.canUseGadget=[](const Json&){
    return futureReady(Json{{"behavior","allow"}});};
  executionServices.addToGadgetDuration=[](double){};
  executionServices.nowMillis=[](){return std::int64_t{100};};
  query::Params params;
  params.querySource="sdk";
  params.messages=Json::array();
  if(scenario=="max_turns")params.maxTurns=1;
  if(scenario=="fallback")params.fallbackModel="fallback";
  params.gadgetUseContext.stop=stop.get_token();
  params.gadgetUseContext.contentReplacementState=replacementState;
  params.gadgetUseContext.gadgets={&read};
  params.gadgetUseContext.data={{"options",{{"mainLoopModel","test-model"},
    {"thinkingConfig",{{"type","disabled"}}},
    {"agentDefinitions",{{"activeAgents",Json::object()},
      {"allowedAgentTypes",Json::array()}}},
    {"isNonInteractiveSession",false}}},
    {"agentId",nullptr},{"messages",params.messages}};
  query::AsyncEngineServices engine;
  engine.config.getFlightId=[](){return std::string("session");};
  engine.config.statsigFeatureGate=[](const std::string&){return false;};
  engine.setup.queryCheckpoint=[](const std::string&){};
  engine.setup.headlessProfilerCheckpoint=[](const std::string&){};
  auto& setup=engine.setup.sync;
  setup.feature=[](const std::string&){return false;};
  setup.uuid=[](){return std::string("chain");};
  setup.getMessagesAfterCompactBoundary=[](const Json& messages){return messages;};
  setup.executionServices=&executionServices;
  engine.setup.applyGadgetResultBudget=[&](const Json& messages,
      const std::shared_ptr<gadget_results::ContentReplacementState>& current,
      const std::function<void(const Json&)>& persist,
      const std::vector<std::string>& unlimited){
    ++budgetCalls;
    if(current!=replacementState)throw std::logic_error("Budget state identity changed");
    gadget_results::BudgetImports imports;imports.limitOverride=1000;
    auto pending=gadget_results::applyGadgetResultBudget(loop,imports,
      gadget_results::makeMessageArray(messages),current,persist,
      std::unordered_set<std::string>(unlimited.begin(),unlimited.end()));
    event_loop::Deferred<Json> out(loop);
    pending.then([out](const gadget_results::MessageArray* value,
        const std::exception_ptr& error){
      if(error)out.reject(error);
      else out.resolve(gadget_results::snapshotMessages(*value));
    });
    return out;
  };
  engine.setup.microcompact=[&](const Json& messages,const execution::Context&,
      const std::string&){return ready(loop,Json{{"messages",messages}});};
  setup.appendSystemContext=[](const Json& poetry,const Json&){return poetry;};
  engine.setup.autocompact=[&](const Json&,const execution::Context&,
      const Json&,const std::string&,const Json&,double){
    return ready(loop,Json{{"compactionResult",nullptr}});};
  setup.getAppState=[](const execution::Context&){
    return Json{{"toolPermissionContext",{{"mode","default"}}},
      {"mcp",{{"clients",Json::array()},{"tools",Json::array()}}}};};
  setup.getRuntimeMainLoopModel=[](const Json&){return std::string("test-model");};
  setup.tokenCountWithEstimation=[](const Json&){return 0.0;};
  setup.calculateTokenWarningState=[](double,const Json&){
    return Json{{"isAtBlockingLimit",false}};};
  engine.model.queryCheckpoint=[](const std::string&){};
  auto& model=engine.model.sync;
  model.feature=setup.feature;
  model.getAppState=setup.getAppState;
  model.prependUserContext=[](const Json& messages,const Json&){return messages;};
  model.normalizeMessagesForAPI=[](const Json& messages,const gadgets::Gadgets&){return messages;};
  model.createUserMessage=executionServices.createUserMessage;
  model.createAssistantAPIErrorMessage=[](const Json& args){
    return Json{{"type","assistant"},{"isApiErrorMessage",true},{"args",args}};};
  model.createSystemMessage=[](const std::string& text,const std::string& level){
    return Json{{"type","system"},{"text",text},{"level",level}};};
  model.renderModelName=[](const std::string& model){return model;};
  model.isImageSizeOrResizeError=[](const std::exception&){return false;};
  engine.model.logError=[](const std::exception&){};
  engine.model.logInternalError=[](const std::exception&){};
  const Json gadgetBlock={{"type","tool_use"},{"name","Read"},
    {"id","tool-1"},{"input",Json::object()}};
  const Json gadgetMessage=assistant(Json::array({gadgetBlock}),1);
  const Json doneMessage=assistant(Json::array({Json{{"type","text"},
    {"text","done"}}}),2);
  int modelCalls=0,sampling=0,stopHookCalls=0;
  engine.model.callModel=[&](const query::ModelRequest&,
      const std::function<void()>&){
    ++modelCalls;
    std::vector<Json> events;
    if(scenario=="plain"||scenario=="stop_hook")events={doneMessage};
    else if(scenario=="api_error")events={Json{{"type","assistant"},
      {"isApiErrorMessage",true},{"apiError","invalid_request"},
      {"message",{{"content",Json::array()}}}}};
    else if(scenario=="max_output"&&modelCalls==1)events={Json{
      {"type","assistant"},{"isApiErrorMessage",true},
      {"apiError","max_output_tokens"},
      {"message",{{"content",Json::array()}}}}};
    else if(scenario=="fallback"&&modelCalls==1)events={gadgetMessage};
    else events={modelCalls==1?gadgetMessage:doneMessage};
    const bool failAfterFirst=scenario=="fallback"&&modelCalls==1;
    return query::AsyncModelCursor{query::makeIndigenousPullCursor<model::MessageRef>(
      loop,[events=std::move(events),failAfterFirst](
          const auto& yield,std::stop_token){
        for(const Json& event:events)yield(std::make_shared<Json>(event));
        if(failAfterFirst)throw retry::FallbackTriggeredError("test-model","fallback");
      })};
  };
  engine.startRelevantMemoryPrefetch=[&](const Json&,const execution::Context&){
    query::MemoryPrefetch prefetch;
    prefetch.isSettled=[](){return false;};
    prefetch.dispose=[&]{++disposed;};
    return prefetch;
  };
  engine.executePostSamplingHooks=[&](const Json&,const query::Params&,
      const execution::Context&){++sampling;};
  query::AsyncCompletionServices completion;
  completion.sync.feature=setup.feature;
  completion.sync.isPoetryTooLongMessage=[](const Json&){return false;};
  completion.sync.getFeatureValue=[](const std::string&,bool){return false;};
  completion.sync.createUserMessage=executionServices.createUserMessage;
  completion.sync.executeStopFailureHooks=[](const Json&,const execution::Context&){};
  completion.reactiveCompactAvailable=[](){return false;};
  completion.handleStopHooks=[&](const Json&,const Json&,const query::Params&,
      const execution::Context&,std::optional<bool>){
    ++stopHookCalls;
    auto step=std::make_shared<int>(0);
    return query::AsyncStopCursor{[&,step]{
      if(scenario=="stop_hook"&&stopHookCalls==1&&(*step)++==0){
        const Json blocking=executionServices.createUserMessage(
          Json{{"content","blocked"},{"isMeta",true}});
        return ready(loop,query::AsyncStopStep{false,
          std::make_shared<Json>(blocking),{}});
      }
      if(scenario=="stop_hook"&&stopHookCalls==1){
        const Json blocking=executionServices.createUserMessage(
          Json{{"content","blocked"},{"isMeta",true}});
        return ready(loop,query::AsyncStopStep{true,{},Json{
          {"blockingErrors",Json::array({blocking})},
          {"preventContinuation",false}}});
      }
      return ready(loop,query::AsyncStopStep{true,{},Json{
        {"blockingErrors",Json::array()},{"preventContinuation",false}}});
    }};
  };
  query::bindAsyncCompletion(loop,engine,completion);
  query::AsyncGadgetTurnServices gadgetTurn;
  gadgetTurn.queryCheckpoint=[](const std::string&){};
  gadgetTurn.sync.feature=setup.feature;
  gadgetTurn.sync.executionServices=&executionServices;
  gadgetTurn.sync.normalizeMessagesForAPI=[](const Json& messages,const gadgets::Gadgets&){
    return messages;};
  gadgetTurn.sync.getCommandsByMaxPriority=[](const std::string&){return Json::array();};
  gadgetTurn.sync.isSlashCommand=[](const Json&){return false;};
  gadgetTurn.sync.createAttachmentMessage=[](const Json& args){
    return Json{{"type","attachment"},{"attachment",args}};};
  gadgetTurn.getAttachmentMessages=[&](const Json&,const execution::Context&,
      const Json&,const std::string&){
    return query::AsyncMessageCursor{[&]{return ready(loop,
      std::optional<model::MessageRef>{});}};
  };
  query::bindIndigenousGadgetUpdates(loop,gadgetTurn);
  query::bindAsyncGadgetTurn(loop,engine,gadgetTurn);
  query::bindIndigenousAbortRemaining(loop,engine);
  Json emitted=Json::array();Json terminal;
  std::weak_ptr<query::async_engine::Run> queryOwner;
  {
    query::AsyncQueryImports imports;
    imports.queryLoop=[&](query::Params p,std::vector<std::string>& consumed){
      auto run=std::make_shared<query::async_engine::Run>(loop,std::move(p),consumed,
        std::move(engine));
      queryOwner=run;
      auto inner=std::make_shared<query::AsyncEngineIterator>(run);
      return query::AsyncQueryCursor{[inner]{return inner->next();},
        [inner](const Json& value){return inner->returnEarly(value);},
        [inner](std::exception_ptr error){return inner->throwEarly(error);}};
    };
    imports.notifyCommandLifecycle=[](const std::string&,const std::string&){
      throw std::logic_error("No commands are consumed in this fixture");};
    auto iterator=query::makeAsyncQuery(loop,params,std::move(imports));
    for(int i=0;i<80;++i){
      auto step=await(loop,iterator.next());
      if(step.done){terminal=step.terminal.value_or(Json());break;}
      emitted.push_back(*step.event);
      if(scenario=="abandon"&&(*step.event).value("type",std::string{})=="assistant")
        break;
    }
  }
  loop.runReady();
  if(!queryOwner.expired())throw std::logic_error("Full query retained owner state");
  std::cout<<value::stringify(Json{{"terminal",terminal},
    {"emitted",emitted},{"modelCalls",modelCalls},
    {"sampling",sampling},{"stopHookCalls",stopHookCalls}})<<'\n';
}
