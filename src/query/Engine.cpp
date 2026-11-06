#include "query/Engine.hpp"
#include <stdexcept>

namespace batbox::query {
namespace {
template<class T>void need(const T& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Unbound query import: ")+name);
}
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
  need(yield,"yield");
  State state;
  state.messages=params.messages;
  state.gadgetUseContext=params.gadgetUseContext;
  state.maxOutputTokensOverride=params.maxOutputTokensOverride;
  std::optional<double> missionBudgetRemaining;
  need(services.startRelevantMemoryPrefetch,"startRelevantMemoryPrefetch");
  MemoryPrefetch memoryPrefetch=services.startRelevantMemoryPrefetch(state.messages,state.gadgetUseContext);
  auto body=[&]()->Json{
  while(true){
    auto prepared=prepareIteration(params,state,services.setup,missionBudgetRemaining,yield);
    if(prepared.terminal)return *prepared.terminal;
    auto modelResult=runModelPass(params,state,prepared,missionBudgetRemaining,services.model,yield);
    if(modelResult.terminal)return *modelResult.terminal;
    if(!modelResult.assistantMessages.empty()){
      need(services.executePostSamplingHooks,"executePostSamplingHooks");
      services.executePostSamplingHooks(concatMessages(prepared.messagesForQuery,
        modelResult.assistantMessages),params,prepared.gadgetUseContext);
    }
    if(prepared.gadgetUseContext.stop.stop_requested()){
      need(services.model.createUserMessage,"createUserMessage");
      yieldMissingGadgetResultBlocks(modelResult.assistantMessages,"Interrupted by user",
        services.model.createUserMessage,yield);
      need(services.gadgetTurn.currentAbortReason,"currentAbortReason");
      if(services.gadgetTurn.currentAbortReason()!="interrupt"){
        need(services.gadgetTurn.createUserInterruptionMessage,"createUserInterruptionMessage");
        yield(services.gadgetTurn.createUserInterruptionMessage(false));
      }
      return Json{{"reason","aborted_streaming"}};
    }
    CompletionDecision decision;
    if(!modelResult.needsFollowUp)
      decision=completeWithoutGadgets(params,state,prepared,modelResult,
        services.completion,yield);
    else decision=executeGadgetTurn(params,state,prepared,modelResult,
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
