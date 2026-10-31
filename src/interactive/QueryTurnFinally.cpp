#include "interactive/QueryTurnInternal.hpp"
#include <algorithm>

namespace batbox::interactive::query_turn {
void Run::finalize(std::exception_ptr error){
  bodyError=std::move(error);
  try{
    if(state->guard.end(*generation))finishCurrent();
    else{maybeRestore();settle();}
  }catch(...){result.reject(std::current_exception());}
}
void Run::finishCurrent(){
  auto& s=*services;
  need(s.now,"now");state->lastQueryCompletionTime=s.now();
  state->skipIdleCheck=false;
  need(s.resetLoadingState,"resetLoadingState");s.resetLoadingState();
  need(s.mrOnTurnComplete,"mrOnTurnComplete");need(s.getMessages,"getMessages");
  auto self=shared_from_this();
  await(s.mrOnTurnComplete(s.getMessages(),request.abort->aborted()),
    [self](const event_loop::Unit&){self->afterTurnComplete();},true);
}
void Run::afterTurnComplete(){
  auto& s=*services;
  need(s.sendBridgeResult,"sendBridgeResult");s.sendBridgeResult();
  std::optional<Json> budget;
  need(s.feature,"feature");
  if(s.feature("TOKEN_BUDGET")){
    need(s.getCurrentTurnTokenBudget,"getCurrentTurnTokenBudget");
    // Keep the separate reads and short-circuit order.
    if(s.getCurrentTurnTokenBudget()){
      const auto positive=s.getCurrentTurnTokenBudget();
      if(positive&&*positive>0&&!request.abort->aborted()){
        need(s.getTurnOutputTokens,"getTurnOutputTokens");
        need(s.getBudgetContinuationCount,"getBudgetContinuationCount");
        const auto tokens=s.getTurnOutputTokens();
        const auto limit=s.getCurrentTurnTokenBudget();
        const auto nudges=s.getBudgetContinuationCount();
        budget=Json{{"tokens",tokens},{"limit",limit?Json(*limit):Json()},
          {"nudges",nudges}};
      }
    }
    need(s.snapshotOutputTokensForTurn,"snapshotOutputTokensForTurn");
    s.snapshotOutputTokensForTurn({});
  }
  need(s.now,"now");
  const auto duration=s.now()-state->loadingStartTime-state->totalPausedMs;
  if((duration>30000||budget)&&!request.abort->aborted()&&!request.proactiveActive){
    need(s.hasRunningSwarmBats,"hasRunningSwarmAgents");
    if(s.hasRunningSwarmBats()){
      if(!state->swarmStartTime)state->swarmStartTime=state->loadingStartTime;
      if(budget)state->swarmBudgetInfo=budget;
    }else{
      need(s.setMessages,"setMessages");need(s.isLoggableMessage,"isLoggableMessage");
      need(s.createTurnDurationMessage,"createTurnDurationMessage");
      auto imports=services;
      s.setMessages([imports,duration,budget](const TurnMessagesRef& old){
        auto messages=std::make_shared<TurnMessages>(*old);
        const auto count=static_cast<std::size_t>(std::count_if(
          old->begin(),old->end(),imports->isLoggableMessage));
        messages->push_back(imports->createTurnDurationMessage(duration,budget,count));
        return messages;
      });
    }
  }
  need(s.clearAbortController,"clearAbortController");s.clearAbortController();
  maybeRestore();settle();
}
void Run::maybeRestore(){
  if(!request.abort->reason.is_string()||request.abort->reason!="user-cancel"||
      state->guard.isActive())return;
  auto& s=*services;
  need(s.getInputValue,"getInputValue");if(!s.getInputValue().empty())return;
  need(s.getCommandQueueLength,"getCommandQueueLength");if(s.getCommandQueueLength()!=0)return;
  need(s.isViewingBatMission,"isViewingAgentTask");if(s.isViewingBatMission())return;
  need(s.getMessages,"getMessages");const auto messages=s.getMessages();
  need(s.selectableUserMessagesFilter,"selectableUserMessagesFilter");
  TurnMessageRef selected;
  for(auto it=messages->rbegin();it!=messages->rend();++it)
    if(s.selectableUserMessagesFilter(*it)){selected=*it;break;}
  if(!selected)return;
  auto index=messages->size();
  while(index>0){--index;if((*messages)[index]==selected)break;}
  need(s.messagesAfterAreOnlySynthetic,"messagesAfterAreOnlySynthetic");
  if(!s.messagesAfterAreOnlySynthetic(messages,index))return;
  need(s.removeLastFromHistory,"removeLastFromHistory");s.removeLastFromHistory();
  need(s.restoreMessageSync,"restoreMessageSync");s.restoreMessageSync(selected);
}
void Run::settle(){
  if(bodyError)result.reject(bodyError);else result.resolve(event_loop::Unit{});
}
} // namespace batbox::interactive::query_turn
