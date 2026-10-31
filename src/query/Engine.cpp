#include "query/Engine.hpp"
#include <stdexcept>

namespace batbox::query {
namespace {
template<class T>void need(const T& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Unbound query import: ")+name);
}
const Json& field(const Json& object,const char* name){return value::property(object,name);}
Json concatMessages(const Json& first,const Json& second){
  Json result=first.is_array()?first:Json::array();
  if(second.is_array())for(const Json& message:second)result.push_back(message);
  return result;
}
}

QuerySuppressedError::QuerySuppressedError(std::exception_ptr disposal,std::exception_ptr prior)
  :std::runtime_error("Memory prefetch disposal failed while handling an earlier query error"),
   disposalError(std::move(disposal)),priorError(std::move(prior)){}

Json queryLoop(const Params& params,std::vector<std::string>& consumedCommandUuids,
               EngineServices& services,const Yield& yield){
  need(yield,"yield");need(services.setup.feature,"feature");
  State state;
  state.messages=params.messages;
  state.gadgetUseContext=params.gadgetUseContext;
  state.maxOutputTokensOverride=params.maxOutputTokensOverride;
  std::optional<BudgetTracker> budgetTracker;
  if(services.setup.feature("TOKEN_BUDGET"))
    budgetTracker=createBudgetTracker(services.completion.budgetDependencies);
  std::optional<double> missionBudgetRemaining;
  const QueryConfig config=buildQueryConfig(services.config);
  need(services.startRelevantMemoryPrefetch,"startRelevantMemoryPrefetch");
  MemoryPrefetch memoryPrefetch=services.startRelevantMemoryPrefetch(state.messages,state.gadgetUseContext);
  auto body=[&]()->Json{
  while(true){
    auto prepared=prepareIteration(params,state,config,services.setup,missionBudgetRemaining,yield);
    if(prepared.terminal)return *prepared.terminal;
    auto modelResult=runModelPass(params,state,prepared,config,missionBudgetRemaining,services.model,yield);
    if(modelResult.terminal)return *modelResult.terminal;
    if(!modelResult.assistantMessages.empty()){
      need(services.executePostSamplingHooks,"executePostSamplingHooks");
      services.executePostSamplingHooks(concatMessages(prepared.messagesForQuery,
        modelResult.assistantMessages),params,prepared.gadgetUseContext);
    }
    if(prepared.gadgetUseContext.stop.stop_requested()){
      if(prepared.streamingGadgetExecutor){
        prepared.streamingGadgetExecutor->getRemainingResults([&](const execution::Update& update){
          if(update.message)yield(*update.message);
        });
      }else{
        need(services.model.createUserMessage,"createUserMessage");
        yieldMissingGadgetResultBlocks(modelResult.assistantMessages,"Interrupted by user",
          services.model.createUserMessage,yield);
      }
      if(services.setup.feature("CHICAGO_MCP") &&
         !value::truthy(field(prepared.gadgetUseContext.data,"agentId")))
        if(services.gadgetTurn.cleanupComputerUseAfterTurn)try{
          services.gadgetTurn.cleanupComputerUseAfterTurn(prepared.gadgetUseContext);
        }catch(...){}
      need(services.gadgetTurn.currentAbortReason,"currentAbortReason");
      if(services.gadgetTurn.currentAbortReason()!="interrupt"){
        need(services.gadgetTurn.createUserInterruptionMessage,"createUserInterruptionMessage");
        yield(services.gadgetTurn.createUserInterruptionMessage(false));
      }
      return Json{{"reason","aborted_streaming"}};
    }
    if(state.pendingGadgetUseSummary){
      const Json summary=state.pendingGadgetUseSummary->get();
      if(value::truthy(summary))yield(summary);
    }
    CompletionDecision decision;
    if(!modelResult.needsFollowUp)
      decision=completeWithoutGadgets(params,state,prepared,modelResult,
        missionBudgetRemaining,budgetTracker?&*budgetTracker:nullptr,
        services.completion,yield);
    else decision=executeGadgetTurn(params,state,prepared,modelResult,config,
      &memoryPrefetch,consumedCommandUuids,services.gadgetTurn,yield);
    if(decision.terminal)return *decision.terminal;
    if(!decision.next)throw std::logic_error("Query iteration returned no transition");
    state=std::move(*decision.next);
  }
  };
  bool disposalAttempted=false;
  try{
    Json terminal=body();
    if(memoryPrefetch.dispose){disposalAttempted=true;memoryPrefetch.dispose();}
    return terminal;
  }catch(...){
    const auto prior=std::current_exception();
    if(!disposalAttempted&&memoryPrefetch.dispose){
      disposalAttempted=true;
      try{memoryPrefetch.dispose();}
      catch(...){throw QuerySuppressedError(std::current_exception(),prior);}
    }
    std::rethrow_exception(prior);
  }
}

Json query(const Params& params,EngineServices& services,const Yield& yield){
  return query(Json(),[&](const Json&,std::vector<std::string>& consumed,const Yield& emit){
    return queryLoop(params,consumed,services,emit);
  },[&](const std::string& uuid,const std::string& state){
    need(services.gadgetTurn.notifyCommandLifecycle,"notifyCommandLifecycle");
    services.gadgetTurn.notifyCommandLifecycle(uuid,state);
  },yield);
}
} // namespace batbox::query
