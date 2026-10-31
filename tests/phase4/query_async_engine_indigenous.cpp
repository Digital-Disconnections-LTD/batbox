#include "query/AsyncEngine.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace batbox;
namespace {
void check(bool condition,const char* message="Async query check failed"){
  if(!condition)throw std::logic_error(message);
}
template<class T>event_loop::Deferred<T> ready(event_loop::Loop& loop,T value){
  event_loop::Deferred<T> out(loop);out.resolve(std::move(value));return out;
}
struct Fixture {
  event_loop::Loop loop;
  query::Params params;
  query::AsyncEngineServices services;
  std::vector<std::string> consumed;
  Json emitted=Json::array();
  int modelCalls=0,gadgetTurns=0,completions=0,sampling=0,disposed=0;
  std::stop_source stop;
  bool abort=false;
  bool withSummary=false;
  bool disposeThrows=false;
  std::optional<event_loop::Deferred<Json>> summaryGate;
  Fixture(bool interrupted=false,bool summary=false):abort(interrupted),withSummary(summary){
    if(withSummary)summaryGate.emplace(loop);
    params.querySource="sdk";params.messages=Json::array();
    params.gadgetUseContext.stop=stop.get_token();
    params.gadgetUseContext.data={{"options",{{"mainLoopModel","model"},
      {"agentDefinitions",{{"activeAgents",Json::object()},
        {"allowedAgentTypes",Json::array()}}}}}};
    services.config.getFlightId=[](){return std::string("session");};
    services.config.statsigFeatureGate=[](const std::string&){return false;};
    auto& setup=services.setup;
    setup.queryCheckpoint=[](const std::string&){};
    setup.headlessProfilerCheckpoint=[](const std::string&){};
    setup.sync.feature=[](const std::string&){return false;};
    setup.sync.uuid=[](){return std::string("chain");};
    setup.sync.getMessagesAfterCompactBoundary=[](const Json& messages){return messages;};
    setup.applyGadgetResultBudget=[this](const Json& messages,
        const std::shared_ptr<gadget_results::ContentReplacementState>&,
        const std::function<void(const Json&)>&,const std::vector<std::string>&){
      return ready(loop,messages);};
    setup.microcompact=[this](const Json& messages,const execution::Context&,
        const std::string&){return ready(loop,Json{{"messages",messages}});};
    setup.sync.appendSystemContext=[](const Json& poetry,const Json&){return poetry;};
    setup.autocompact=[this](const Json&,const execution::Context&,
        const Json&,const std::string&,const Json&,double){
      return ready(loop,Json{{"compactionResult",nullptr}});};
    setup.sync.getAppState=[](const execution::Context&){
      return Json{{"toolPermissionContext",{{"mode","default"}}},
        {"mcp",{{"clients",Json::array()},{"tools",Json::array()}}}};};
    setup.sync.getRuntimeMainLoopModel=[](const Json&){return std::string("model");};
    setup.sync.tokenCountWithEstimation=[](const Json&){return 0.0;};
    setup.sync.calculateTokenWarningState=[](double,const Json&){
      return Json{{"isAtBlockingLimit",false}};};
    auto& model=services.model;
    model.queryCheckpoint=[](const std::string&){};
    model.sync.feature=[](const std::string&){return false;};
    model.sync.prependUserContext=[](const Json& messages,const Json&){return messages;};
    model.sync.getAppState=setup.sync.getAppState;
    model.sync.createUserMessage=[](const Json& args){return Json{{"type","user"},{"args",args}};};
    model.callModel=[this](const query::ModelRequest&,
        const std::function<void()>&){
      const int call=++modelCalls;
      auto index=std::make_shared<int>(0);
      return query::AsyncModelCursor{[this,call,index]{
        if((*index)++==0){
          Json content=call==1?Json::array({Json{{"type","tool_use"},
              {"id","tool-1"},{"name","Read"},{"input",Json::object()}}}):
            Json::array({Json{{"type","text"},{"text","done"}}});
          auto message=std::make_shared<Json>(Json{{"type","assistant"},
            {"uuid",call==1?"assistant-1":"assistant-2"},
            {"message",{{"content",content}}}});
          return ready(loop,std::optional<model::MessageRef>(message));
        }
        return ready(loop,std::optional<model::MessageRef>{});
      }};
    };
    services.startRelevantMemoryPrefetch=[this](const Json&,const execution::Context&){
      query::MemoryPrefetch out;out.dispose=[this]{
        ++disposed;if(disposeThrows)throw std::runtime_error("dispose failed");
      };return out;};
    services.executePostSamplingHooks=[this](const Json&,const query::Params&,
        const execution::Context&){++sampling;};
    services.gadgetTurn.currentAbortReason=[](){return std::string("timeout");};
    services.gadgetTurn.createUserInterruptionMessage=[](bool){
      return Json{{"type","user"},{"interrupted",true}};};
    services.awaitPendingGadgetUseSummary=[this](const query::State&){
      check(summaryGate.has_value());return *summaryGate;};
    services.completeWithoutGadgets=[this](const query::Params&,const query::State&,
        std::shared_ptr<query::PreparedIteration>,std::shared_ptr<query::ModelPassResult>,
        std::optional<double>&,
        query::BudgetTracker*){
      ++completions;
      return query::AsyncDecisionCursor{[this]{
        auto value=std::make_shared<query::CompletionDecision>();
        value->terminal=Json{{"reason","completed"}};
        return ready(loop,query::AsyncDecisionStep{true,{},value});
      }};
    };
    services.executeGadgetTurn=[this](const query::Params&,const query::State& state,
        std::shared_ptr<query::PreparedIteration>,std::shared_ptr<query::ModelPassResult>,
        const query::QueryConfig&,
        query::MemoryPrefetch*,std::vector<std::string>&){
      ++gadgetTurns;
      auto index=std::make_shared<int>(0);
      return query::AsyncDecisionCursor{[this,state,index]{
        if((*index)++==0){
          auto message=std::make_shared<Json>(Json{{"type","user"},
            {"message",{{"content",Json::array({Json{{"type","tool_result"},
              {"tool_use_id","tool-1"},{"content","read done"}}})}}}});
          return ready(loop,query::AsyncDecisionStep{false,message,{}});
        }
        auto next=state;next.turnCount++;
        if(withSummary){
          std::promise<Json> promise;
          promise.set_value(Json{{"type","tool_use_summary"}});
          next.pendingGadgetUseSummary=promise.get_future().share();
        }
        auto value=std::make_shared<query::CompletionDecision>();value->next=next;
        return ready(loop,query::AsyncDecisionStep{true,{},value});
      }};
    };
  }
  query::AsyncEngineStep next(query::AsyncEngineIterator& iterator){
    auto pending=iterator.next();
    std::optional<query::AsyncEngineStep> step;std::exception_ptr error;
    pending.then([&](const query::AsyncEngineStep* value,const std::exception_ptr& rejected){
      error=rejected;if(value)step=*value;});
    loop.runReady();if(error)std::rethrow_exception(error);
    if(!step)throw std::logic_error("query next() did not settle");
    if(!step->done)emitted.push_back(*step->event);
    return *step;
  }
};
void finish(Fixture& fixture,query::AsyncEngineIterator& iterator){
  for(int step=0;step<24;++step)if(fixture.next(iterator).done)return;
  throw std::logic_error("Query did not finish within the fixture's expected steps");
}
void compactContextRecovery(){
  for(const bool enabled:{false,true}){
    Fixture fx;
    fx.params.userContext={{"projectRecovery","old target"},{"agentNote","preserved"}};
    int setups=0,refreshes=0;
    Json contexts=Json::array();
    fx.services.setup.autocompact=[&](const Json&,const execution::Context&,
        const Json&,const std::string&,const Json&,double){
      return ready(fx.loop,Json{{"compactionResult",++setups==2?
        Json{{"summary","compacted"}}:Json()}});
    };
    fx.services.setup.sync.buildPostCompactMessages=[](const Json&){return Json::array();};
    fx.services.model.sync.prependUserContext=[&](const Json& messages,const Json& context){
      contexts.push_back(context);return messages;
    };
    if(enabled)fx.services.refreshUserContextAfterCompact=[&](const Json& previous,
        const execution::Context&){
      ++refreshes;
      auto context=previous;context["projectRecovery"]="new target";
      return ready(fx.loop,std::move(context));
    };
    auto iterator=query::makeAsyncQueryLoop(fx.loop,fx.params,fx.consumed,fx.services);
    finish(fx,iterator);
    check(contexts.size()==2,"Automatic compact fixture made two model calls");
    check(contexts[0]["projectRecovery"]=="old target","Pre-compact context remains unchanged");
    check(contexts[1]["projectRecovery"]==(enabled?"new target":"old target"),
      "Automatic compact replaces the next request's context only when bound");
    check(contexts[1]["agentNote"]=="preserved"&&refreshes==(enabled?1:0),
      "Refresh happens once and retains query additions");
  }
  {
    Fixture fx;
    fx.params.userContext={{"projectRecovery","old target"}};
    int refreshes=0;
    Json contexts=Json::array();
    fx.services.model.sync.prependUserContext=[&](const Json& messages,const Json& context){
      contexts.push_back(context);return messages;
    };
    fx.services.model.callModel=[&](const query::ModelRequest&,const std::function<void()>&){
      ++fx.modelCalls;
      auto sent=std::make_shared<bool>(false);
      return query::AsyncModelCursor{[&,sent]{
        if(*sent)return ready(fx.loop,std::optional<model::MessageRef>{});
        *sent=true;
        auto message=std::make_shared<Json>(Json{{"type","assistant"},
          {"message",{{"content",Json::array({Json{{"type","text"},{"text","result"}}})}}}});
        return ready(fx.loop,std::optional<model::MessageRef>(message));
      }};
    };
    fx.services.completeWithoutGadgets=[&](const query::Params&,const query::State& state,
        std::shared_ptr<query::PreparedIteration>,std::shared_ptr<query::ModelPassResult>,
        std::optional<double>&,query::BudgetTracker*){
      const bool retry=++fx.completions==1;
      return query::AsyncDecisionCursor{[&,state,retry]{
        auto value=std::make_shared<query::CompletionDecision>();
        if(retry){
          auto next=state;next.hasAttemptedReactiveCompact=true;
          next.transition={{"reason","reactive_compact_retry"}};
          value->next=std::move(next);
        }else value->terminal=Json{{"reason","completed"}};
        return ready(fx.loop,query::AsyncDecisionStep{true,{},value});
      }};
    };
    fx.services.refreshUserContextAfterCompact=[&](const Json& previous,const execution::Context&){
      ++refreshes;auto context=previous;context["projectRecovery"]="reactively refreshed";
      return ready(fx.loop,std::move(context));
    };
    auto iterator=query::makeAsyncQueryLoop(fx.loop,fx.params,fx.consumed,fx.services);
    finish(fx,iterator);
    check(refreshes==1&&contexts.size()==2&&
      contexts[1]["projectRecovery"]=="reactively refreshed",
      "Reactive compact refreshes context before retrying the model");
  }
  {
    Fixture fx;
    event_loop::Deferred<Json> gate(fx.loop);
    fx.services.setup.autocompact=[&](const Json&,const execution::Context&,
        const Json&,const std::string&,const Json&,double){
      return ready(fx.loop,Json{{"compactionResult",Json{{"summary","compacted"}}}});
    };
    fx.services.setup.sync.buildPostCompactMessages=[](const Json&){return Json::array();};
    fx.services.refreshUserContextAfterCompact=[&](const Json&,const execution::Context&){return gate;};
    auto iterator=query::makeAsyncQueryLoop(fx.loop,fx.params,fx.consumed,fx.services);
    fx.next(iterator);
    bool settled=false;std::exception_ptr error;
    iterator.next().then([&](const query::AsyncEngineStep*,const std::exception_ptr& rejected){
      settled=true;error=rejected;
    });
    fx.loop.runReady();
    check(!settled&&fx.modelCalls==0,"The next model request waits for context recovery");
    gate.reject(std::make_exception_ptr(std::runtime_error("context load failed")));
    fx.loop.runReady();
    check(settled&&error&&fx.modelCalls==0&&fx.disposed==1,
      "Failed recovery rejects the query and disposes its prefetch");
  }
}
}
int main(){
  compactContextRecovery();
  {
    Fixture fx;
    auto original=std::make_shared<Json>(Json{{"type","assistant"},
      {"message",{{"content",Json::array({Json{{"type","text"},{"text","partial"}}})}}}});
    std::weak_ptr<Json> retained=original;
    fx.services.model.callModel=[&](const query::ModelRequest&,
        const std::function<void()>& fallback){
      auto index=std::make_shared<int>(0);
      return query::AsyncModelCursor{[&,fallback,index]{
        if((*index)++==0)return ready(fx.loop,std::optional<model::MessageRef>(original));
        if(*index==2){
          fallback();
          auto replacement=std::make_shared<Json>(*original);
          (*replacement)["message"]["content"][0]["text"]="replacement";
          return ready(fx.loop,std::optional<model::MessageRef>(replacement));
        }
        return ready(fx.loop,std::optional<model::MessageRef>{});
      }};
    };
    auto iterator=query::makeAsyncQueryLoop(fx.loop,fx.params,fx.consumed,fx.services);
    check(!fx.next(iterator).tombstoneTarget);
    auto first=fx.next(iterator);
    auto tombstone=fx.next(iterator);
    check((*tombstone.event)["type"]=="tombstone");
    check(tombstone.tombstoneTarget==first.event&&first.event==original);
    check(!fx.next(iterator).tombstoneTarget);
    check(fx.next(iterator).done);
    first={};original.reset();
    check(retained.lock()==tombstone.tombstoneTarget);
    tombstone={};
    check(retained.expired());
  }
  {
    Fixture fx;
    auto iterator=query::makeAsyncQueryLoop(fx.loop,fx.params,fx.consumed,fx.services);
    auto first=fx.next(iterator);
    check(!first.done&&(*first.event)["type"]=="stream_request_start");
    check(fx.modelCalls==0&&fx.gadgetTurns==0);
    auto assistant=fx.next(iterator);
    check(!assistant.done&&(*assistant.event)["type"]=="assistant");
    check(fx.gadgetTurns==0);
    auto gadgetResult=fx.next(iterator);
    check(!gadgetResult.done&&(*gadgetResult.event)["type"]=="user");
    check(fx.gadgetTurns==1&&fx.modelCalls==1);
    auto secondStart=fx.next(iterator);
    check(!secondStart.done&&(*secondStart.event)["type"]=="stream_request_start");
    check(fx.modelCalls==1);
    auto secondAssistant=fx.next(iterator);
    check(!secondAssistant.done&&(*secondAssistant.event)["type"]=="assistant");
    auto done=fx.next(iterator);
    check(done.done&&done.terminal&&done.terminal->at("reason")=="completed");
    check(fx.modelCalls==2&&fx.gadgetTurns==1&&fx.completions==1);
    check(fx.sampling==2&&fx.disposed==1);
  }
  {
    Fixture fx(true);
    auto iterator=query::makeAsyncQueryLoop(fx.loop,fx.params,fx.consumed,fx.services);
    fx.next(iterator);
    auto assistant=fx.next(iterator);
    check((*assistant.event)["type"]=="assistant");
    fx.stop.request_stop();
    auto missing=fx.next(iterator);
    check(!missing.done&&(*missing.event)["type"]=="user");
    auto interruption=fx.next(iterator);
    check(!interruption.done&&(*interruption.event)["interrupted"]==true);
    auto done=fx.next(iterator);
    check(done.done&&done.terminal&&done.terminal->at("reason")=="aborted_streaming");
    check(fx.gadgetTurns==0&&fx.completions==0&&fx.disposed==1);
  }
  {
    Fixture fx(false,true);
    auto iterator=query::makeAsyncQueryLoop(fx.loop,fx.params,fx.consumed,fx.services);
    for(int i=0;i<5;++i)fx.next(iterator);
    check(fx.modelCalls==2&&fx.completions==0);
    auto pending=iterator.next();
    std::optional<query::AsyncEngineStep> observed;
    pending.then([&](const query::AsyncEngineStep* step,const std::exception_ptr& error){
      check(!error);observed=*step;});
    fx.loop.runReady();
    check(!observed&&fx.completions==0);
    fx.summaryGate->resolve(Json{{"type","tool_use_summary"}});
    fx.loop.runReady();
    check(observed&&!observed->done&&(*observed->event)["type"]=="tool_use_summary");
    check(fx.completions==0);
    auto done=fx.next(iterator);
    check(done.done&&done.terminal&&done.terminal->at("reason")=="completed");
    check(fx.completions==1&&fx.disposed==1);
  }
  {
    Fixture fx;fx.disposeThrows=true;
    fx.services.setup.applyGadgetResultBudget=[&](const Json&,
        const std::shared_ptr<gadget_results::ContentReplacementState>&,
        const std::function<void(const Json&)>&,const std::vector<std::string>&){
      event_loop::Deferred<Json> pending(fx.loop);
      pending.reject(std::make_exception_ptr(std::runtime_error("budget failed")));
      return pending;
    };
    auto iterator=query::makeAsyncQueryLoop(fx.loop,fx.params,fx.consumed,fx.services);
    fx.next(iterator);
    bool suppressed=false;
    try{fx.next(iterator);}catch(const query::QuerySuppressedError& error){
      suppressed=error.disposalError&&error.priorError;
    }
    check(suppressed&&fx.disposed==1);
  }
  std::cout<<"owner-loop setup/model/tool decisions, compact context recovery, demand, abort and disposal passed\n";
}
