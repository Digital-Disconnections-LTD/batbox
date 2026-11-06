#include "query/AsyncGadgetTurnInternal.hpp"

namespace batbox::query::async_gadget_turn {
void Run::emitAbortInterruption(){
  need(services.sync.currentAbortReason,"currentAbortReason");
  if(services.sync.currentAbortReason()!="interrupt"){
    need(services.sync.createUserInterruptionMessage,
      "createUserInterruptionMessage");
    emit(std::make_shared<Json>(services.sync.createUserInterruptionMessage(true)),
      Stage::AbortMaxTurns);
  }else stage=Stage::AbortMaxTurns;
}
void Run::abortMaxTurns(){
  const int nextTurn=state.turnCount+1;
  if(params.maxTurns&&*params.maxTurns!=0&&nextTurn>*params.maxTurns){
    need(services.sync.createAttachmentMessage,"createAttachmentMessage");
    emit(std::make_shared<Json>(services.sync.createAttachmentMessage(
      Json{{"type","max_turns_reached"},{"maxTurns",*params.maxTurns},
        {"turnCount",nextTurn}})),Stage::AfterAbortMax);
  }else stage=Stage::AbortTerminal;
}
} // namespace batbox::query::async_gadget_turn
