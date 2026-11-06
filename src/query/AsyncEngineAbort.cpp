#include "query/AsyncEngineInternal.hpp"

namespace batbox::query::async_engine {
namespace {
std::vector<Json> missingGadgetArgs(const Json& assistants){
  std::vector<Json> out;
  if(!assistants.is_array())return out;
  for(const Json& assistant:assistants){
    const Json& content=field(field(assistant,"message"),"content");
    if(!content.is_array())throw std::invalid_argument("Assistant content must be an array");
    for(const Json& block:content)if(field(block,"type")=="tool_use"){
      const Json result={{"type","tool_result"},
        {"content","Interrupted by user"},{"is_error",true},
        {"tool_use_id",field(block,"id")}};
      out.push_back(Json{{"content",Json::array({result})},
        {"toolUseResult","Interrupted by user"},
        {"sourceToolAssistantUUID",field(assistant,"uuid")}});
    }
  }
  return out;
}
}
void Run::startAbort(){
  missingArgs=missingGadgetArgs(modelResult->assistantMessages);
  missingIndex=0;stage=Stage::AbortMissing;
}
void Run::emitMissing(){
  if(missingIndex<missingArgs.size()){
    need(services.model.sync.createUserMessage,"createUserMessage");
    emit(std::make_shared<Json>(services.model.sync.createUserMessage(
      missingArgs[missingIndex++])),Stage::AbortMissing);
    return;
  }
  stage=Stage::AbortInterruption;
}
void Run::emitAbortInterruption(){
  need(services.gadgetTurn.currentAbortReason,"currentAbortReason");
  if(services.gadgetTurn.currentAbortReason()!="interrupt"){
    need(services.gadgetTurn.createUserInterruptionMessage,
      "createUserInterruptionMessage");
    emit(std::make_shared<Json>(services.gadgetTurn.createUserInterruptionMessage(false)),
      Stage::AbortTerminal);
  }else stage=Stage::AbortTerminal;
}
} // namespace batbox::query::async_engine
