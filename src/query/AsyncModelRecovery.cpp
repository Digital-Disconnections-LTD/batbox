#include "query/AsyncModelInternal.hpp"
#include "retry/Retry.hpp"

namespace batbox::query::async_model {
namespace {
std::vector<Json> missingResultArgs(const Json& assistants,
  const std::string& errorMessage){
  std::vector<Json> args;
  if(!assistants.is_array())return args;
  for(const Json& assistant:assistants){
    const Json& content=field(field(assistant,"message"),"content");
    if(!content.is_array())throw std::invalid_argument("Assistant message content must be an array");
    for(const Json& block:content)if(field(block,"type")=="tool_use"){
      const Json result={{"type","tool_result"},{"content",errorMessage},
        {"is_error",true},{"tool_use_id",field(block,"id")}};
      args.push_back(Json{{"content",Json::array({result})},
        {"toolUseResult",errorMessage},{"sourceToolAssistantUUID",field(assistant,"uuid")}});
    }
  }
  return args;
}
}
void Run::onStreamError(std::exception_ptr error,bool allowFallback){
  try{std::rethrow_exception(error);}
  catch(const retry::FallbackTriggeredError& fallback){
    if(allowFallback&&params.fallbackModel){
      handlingError=true;
      prepared->currentModel=*params.fallbackModel;
      fallbackNames=std::pair{fallback.originalModel,fallback.fallbackModel};
      errorMessage="Model fallback triggered";
      syncAssistantValues();
      missingArgs=missingResultArgs(result.assistantMessages,errorMessage);
      missingIndex=0;stage=Stage::EmitMissing;
      return;
    }
    // Without a configured fallback, propagate to the outer catch.
    errorMessage=fallback.what();
  }
  catch(const std::exception& ordinary){errorMessage=ordinary.what();}
  catch(...){errorMessage="Unknown model error";}
  fallbackNames.reset();
  handlingError=true;
  syncAssistantValues();
  need(services.logError,"logError");
  try{std::rethrow_exception(error);}
  catch(const std::exception& ordinary){services.logError(ordinary);}
  catch(...){const std::runtime_error unknown(errorMessage);services.logError(unknown);}
  need(services.sync.isImageSizeOrResizeError,"isImageSizeOrResizeError");
  bool imageError=false;
  try{std::rethrow_exception(error);}
  catch(const std::exception& ordinary){
    imageError=services.sync.isImageSizeOrResizeError(ordinary);
  }catch(...){}
  result.error=error;
  if(imageError){
    result.terminal=Json{{"reason","image_error"}};
    stage=Stage::EmitFailureMessage;
    return;
  }
  missingArgs=missingResultArgs(result.assistantMessages,errorMessage);
  missingIndex=0;
  // Error's enumerable JSON view is {}, while result.error retains the indigenous
  // exception identity for the terminal's non-JSON sidecar.
  result.terminal=Json{{"reason","model_error"},{"error",Json::object()}};
  stage=Stage::EmitMissing;
}
void Run::emitMissing(){
  if(missingIndex<missingArgs.size()){
    need(services.sync.createUserMessage,"createUserMessage");
    emit(std::make_shared<Json>(services.sync.createUserMessage(
      missingArgs[missingIndex++])),Stage::EmitMissing);
    return;
  }
  stage=Stage::AfterMissing;
}
void Run::afterMissing(){
  if(fallbackNames){
    result.assistantMessages=Json::array();result.assistantRefs.clear();
    result.gadgetResults=Json::array();result.gadgetUseBlocks=Json::array();
    result.needsFollowUp=false;
    if(prepared->streamingGadgetExecutor){
      prepared->streamingGadgetExecutor->discard();
      prepared->discardedExecutors.push_back(std::move(prepared->streamingGadgetExecutor));
      need(prepared->executionServices,"executionServices");
      prepared->streamingGadgetExecutor=std::make_unique<execution::StreamingGadgetExecutor>(
        prepared->gadgetUseContext,*prepared->executionServices);
    }
    prepared->gadgetUseContext.data["options"]["mainLoopModel"]=*params.fallbackModel;
    if(config.internalUser){
      need(services.sync.stripSignatureBlocks,"stripSignatureBlocks");
      prepared->messagesForQuery=services.sync.stripSignatureBlocks(prepared->messagesForQuery);
    }
    stage=Stage::EmitFallbackSystem;
  }else stage=Stage::EmitFailureMessage;
}
void Run::emitFallbackSystem(){
  need(services.sync.createSystemMessage,"createSystemMessage");
  need(services.sync.renderModelName,"renderModelName");
  const auto& [original,actual]=*fallbackNames;
  emit(std::make_shared<Json>(services.sync.createSystemMessage(
    "Switched to "+services.sync.renderModelName(actual)+
    " due to high demand for "+services.sync.renderModelName(original),"warning")),
    Stage::AfterFallbackSystem);
}
void Run::afterFallbackSystem(){
  fallbackNames.reset();missingArgs.clear();errorMessage.clear();
  stage=Stage::StartAttempt;
}
void Run::emitFailureMessage(){
  need(services.sync.createAssistantAPIErrorMessage,"createAssistantAPIErrorMessage");
  emit(std::make_shared<Json>(services.sync.createAssistantAPIErrorMessage(
    Json{{"content",errorMessage}})),Stage::AfterFailureMessage);
}
void Run::afterFailureMessage(){
  if(result.terminal && field(*result.terminal,"reason")=="model_error"){
    need(services.logInternalError,"logInternalError");
    try{std::rethrow_exception(result.error);}
    catch(const std::exception& ordinary){services.logInternalError(ordinary);}
    catch(...){const std::runtime_error unknown(errorMessage);services.logInternalError(unknown);}
  }
  finish();
}
} // namespace batbox::query::async_model
