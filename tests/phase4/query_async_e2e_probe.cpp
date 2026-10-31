#include "query/AsyncCompletion.hpp"
#include "query/AsyncEngineInternal.hpp"
#include "query/AsyncGadgetTurn.hpp"
#include "query/IndigenousModelCursor.hpp"
#include "query/OwnerModelCursor.hpp"
#include "query/IndigenousGadgetCursor.hpp"
#include "gadget_results/Budget.hpp"
#include "query_async_e2e_model_fixture.hpp"
#include "../phase3/BashAsyncFixture.hpp"
#include <cassert>
#include <filesystem>
#include <future>
#include <iostream>
#include <unordered_set>
#include <unistd.h>

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
}
int main(int argc,char** argv){
  if(argc<2)return 2;
  event_loop::Loop loop;
  const bool useBash=argc>2&&std::string(argv[2])=="bash";
  const auto bashRoot=std::filesystem::temp_directory_path()/
    ("batbox-query-bash-"+std::to_string(::getpid()));
  auto bash=useBash?test_support::BashAsyncFixture::create(loop,bashRoot):
    std::shared_ptr<test_support::BashAsyncFixture>{};
  QueryE2EModelFixture modelFixture;modelFixture.baseUrl=argv[1];
  modelFixture.owner=&loop;
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
    gadgets::GadgetResult result;result.data={{"text","file contents"}};
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
  executionServices.markGadgetUseComplete=[&](const std::string&){++gadgetCalls;};
  query::Params params;
  params.querySource="sdk";
  params.messages=Json::array({Json{{"type","user"},
    {"message",{{"content",Json::array({Json{{"type","text"},
      {"text","Read a file and then answer."}}})}}}}});
  params.gadgetUseContext.stop=stop.get_token();
  params.gadgetUseContext.contentReplacementState=replacementState;
  params.gadgetUseContext.gadgets={useBash?&bash->gadget:&read};
  params.gadgetUseContext.data={{"options",{{"mainLoopModel","batbox-model"},
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
  setup.getRuntimeMainLoopModel=[](const Json&){return std::string("batbox-model");};
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
    return Json{{"type","assistant"},{"apiError","error"},{"args",args}};};
  model.isImageSizeOrResizeError=[](const std::exception&){return false;};
  engine.model.logError=[](const std::exception&){};
  engine.model.logInternalError=[](const std::exception&){};
  engine.model.callModel=[&](const query::ModelRequest& request,
      const std::function<void()>& fallback){
    model::PreflightInputs input;
    input.messages=request.messages;
    input.systemPoetry=request.systemPoetry;
    input.thinkingConfig=request.thinkingConfig;
    input.gadgets=Json::array({Json{{"name",useBash?"Bash":"Read"},
      {"description",useBash?"Run shell command":"Read file"},
      {"input_schema",{{"type","object"}}}}});
    input.options=request.options;
    auto services=modelFixture.makeServices();
    services.signal=request.signal;
    query::OwnerPreflightAwaits awaits;
    const auto search=services.preflight.isGadgetSearchEnabled;
    awaits.gadgetSearchEnabled=[&loop,search](const std::string& model,
        const Json& gadgets,const Json& options){
      return ready(loop,search(model,gadgets,options));
    };
    const auto schema=services.preflight.gadgetToAPISchema;
    awaits.gadgetToAPISchema=[&loop,schema](const Json& gadget,bool defer,
        const Json& allGadgets,const Json& options){
      return ready(loop,schema(gadget,defer,allGadgets,options));
    };
    return query::makeOwnerSegmentQueryModelCursor(loop,std::move(input),
      std::move(services),std::move(awaits),fallback);
  };
  engine.startRelevantMemoryPrefetch=[&](const Json&,const execution::Context&){
    query::MemoryPrefetch prefetch;
    prefetch.isSettled=[](){return false;};
    prefetch.dispose=[&]{++disposed;};
    return prefetch;
  };
  engine.executePostSamplingHooks=[](const Json&,const query::Params&,
      const execution::Context&){};
  query::AsyncCompletionServices completion;
  completion.sync.feature=setup.feature;
  completion.reactiveCompactAvailable=[](){return false;};
  completion.handleStopHooks=[&](const Json&,const Json&,const query::Params&,
      const execution::Context&,std::optional<bool>){
    return query::AsyncStopCursor{[&]{return ready(loop,
      query::AsyncStopStep{true,{},Json{{"blockingErrors",Json::array()},
        {"preventContinuation",false}}});}};
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
  std::vector<std::string> consumed;
  Json emitted=Json::array();Json terminal;
  std::weak_ptr<query::async_engine::Run> queryOwner;
  {
    auto run=std::make_shared<query::async_engine::Run>(loop,params,consumed,
      std::move(engine));
    queryOwner=run;
    query::AsyncEngineIterator iterator(run);
    run.reset();
    for(int i=0;i<80;++i){
      auto step=await(loop,iterator.next());
      if(step.done){terminal=step.terminal.value_or(Json());break;}
      emitted.push_back(*step.event);
    }
  }
  loop.runReady();
  if(!queryOwner.expired())throw std::logic_error("Completed query retained owner state");
  Json types=Json::array();
  for(const Json& value:emitted)types.push_back(value::property(value,"type"));
  std::cout<<Json{{"terminal",terminal},{"types",types},
    {"emitted",emitted},{"toolCalls",gadgetCalls},{"budgetCalls",budgetCalls},
    {"disposed",disposed},{"costCalls",modelFixture.costCalls},
    {"lastMainRequestId",modelFixture.lastMainRequestId.value_or("")}}.dump()<<'\n';
  bash.reset();
  if(useBash)std::filesystem::remove_all(bashRoot);
}
