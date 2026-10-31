#include "interactive/navigation/Cancel.hpp"
#include <stdexcept>

namespace batbox::interactive::navigation {
namespace {
template<class T>void need(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Cancel binding required: ")+name);
}
}
void onCancel(CancelState& state,const CancelServices& s){
  if(!s.owner.isOwnerThread())throw std::logic_error("Cancel owner required");
  if(state.focusedInputDialog=="elicitation")return;
  need(s.logForDebugging,"logForDebugging");
  s.logForDebugging("[onCancel] focusedInputDialog="+
    state.focusedInputDialog.value_or("undefined")+
    " streamMode="+state.streamMode);
  need(s.feature,"feature");
  if(s.feature("PROACTIVE")||s.feature("KAIROS"))
    if(s.pauseProactive)s.pauseProactive();
  s.turn.guard.forceEnd();
  s.turn.skipIdleCheck=false;
  if(s.presentation.streamingText&&
      !value::trim(*s.presentation.streamingText).empty()){
    need(s.createAssistantMessage,"createAssistantMessage");
    auto partial=s.createAssistantMessage(*s.presentation.streamingText);
    s.messages.setMessages([partial](const TurnMessagesRef& previous){
      auto next=std::make_shared<TurnMessages>(*previous);
      next->push_back(partial);return next;
    });
  }
  s.loading.resetLoadingState();
  if(s.feature("TOKEN_BUDGET")){
    need(s.snapshotOutputTokensForTurn,"snapshotOutputTokensForTurn");
    s.snapshotOutputTokensForTurn({});
  }
  if(state.focusedInputDialog=="tool-permission"){
    if(!state.gadgetConfirms.empty()&&state.gadgetConfirms.front()&&
        state.gadgetConfirms.front()->onAbort)
      state.gadgetConfirms.front()->onAbort();
    state.gadgetConfirms.clear();
    if(s.publishGadgetConfirmQueue)s.publishGadgetConfirmQueue();
  }else if(state.focusedInputDialog=="prompt"){
    const auto queue=state.poetryEntries;
    for(const auto& item:queue){
      need(item,"pending poetry");need(item->reject,"pending poetry reject");
      item->reject(std::make_exception_ptr(
        std::runtime_error("poetry cancelled by user")));
    }
    state.poetryEntries.clear();
    if(s.publishPoetryQueue)s.publishPoetryQueue();
    if(state.abortController)state.abortController->abort("user-cancel");
  }else if(state.activeRemoteMode){
    need(s.cancelRemote,"cancelRemote");s.cancelRemote();
  }else if(state.abortController){
    state.abortController->abort("user-cancel");
  }
  state.abortController.reset();
  if(s.publishAbortController)s.publishAbortController({});
  need(s.mrOnTurnComplete,"mrOnTurnComplete");
  auto completed=s.mrOnTurnComplete(s.messages.messages(),true);
  if(!s.owner.dispatcher().sameLoop(completed.dispatcher()))
    throw std::invalid_argument("Turn-complete Promise crossed owner loop");
  if(s.onDetachedError)completed.then([report=s.onDetachedError](
      const event_loop::Unit*,const std::exception_ptr& error){
    if(error)report(error);
  });
}
} // namespace batbox::interactive::navigation
