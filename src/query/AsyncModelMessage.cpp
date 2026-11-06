#include "query/AsyncModelInternal.hpp"

namespace batbox::query::async_model {
namespace {
model::MessageRef cloneForYield(const model::MessageRef& original,
  const execution::Context& context){
  const Json& message=*original;
  if(field(message,"type")!="assistant")return original;
  const Json& content=field(field(message,"message"),"content");
  if(!content.is_array())throw std::invalid_argument("Assistant content must be an array");
  model::MessageRef clone;
  for(std::size_t index=0;index<content.size();++index){
    const Json& block=content[index];
    const Json& input=field(block,"input");
    if(field(block,"type")!="tool_use"||!input.is_object())continue;
    const auto* gadget=gadgets::findGadgetByName(context.gadgets,stringField(block,"name"));
    if(!gadget||!gadget->backfillObservableInput)continue;
    Json copy=input;
    gadget->backfillObservableInput(copy);
    bool added=false;
    for(const auto& [key,value]:copy.items())if(!input.contains(key)){added=true;break;}
    if(added){
      if(!clone)clone=std::make_shared<Json>(message);
      (*clone)["message"]["content"][index]["input"]=std::move(copy);
    }
  }
  return clone?clone:original;
}
}
void Run::handleMessage(){
  if(streamingFallbackOccurred.load()){
    syncAssistantValues();
    tombstones=result.assistantRefs;
    tombstoneIndex=0;
    stage=Stage::AfterTombstones;
    return;
  }
  afterTombstones();
}
void Run::afterTombstones(){
  if(tombstoneIndex<tombstones.size()){
    const auto target=tombstones[tombstoneIndex++];
    emit(std::make_shared<Json>(Json{{"type","tombstone"},
      {"message",*target}}),Stage::AfterTombstones,target);
    return;
  }
  if(streamingFallbackOccurred.load()){
    result.assistantMessages=Json::array();result.assistantRefs.clear();
    result.gadgetResults=Json::array();result.gadgetUseBlocks=Json::array();
    result.needsFollowUp=false;
  }
  tombstones.clear();
  const auto& message=*currentMessage;
  currentEmission=cloneForYield(message,prepared->gadgetUseContext);
  if(!isWithheldMaxOutputTokens(*message)){
    emit(currentEmission,Stage::AfterAssistantYield);return;
  }
  stage=Stage::AfterAssistantYield;
}
void Run::afterAssistantYield(){
  const auto& message=*currentMessage;
  if(field(*message,"type")=="assistant"){
    result.assistantMessages.push_back(*message);
    result.assistantRefs.push_back(message);
    const Json& content=field(field(*message,"message"),"content");
    if(!content.is_array())throw std::invalid_argument("Assistant content must be an array");
    for(const Json& block:content)if(field(block,"type")=="tool_use"){
      result.gadgetUseBlocks.push_back(block);
      result.needsFollowUp=true;
    }
  }
  currentMessage.reset();currentEmission.reset();
  requestMessage();
}
} // namespace batbox::query::async_model
