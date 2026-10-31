#include "interactive/message_view/MessagePlan.hpp"
#include <stdexcept>

namespace batbox::interactive::message_view {
namespace {
void need(const auto& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Message block import required: ")+name);
}
}
BlockPlan prepareUserBlock(const GadgetBlockRef& block,const Json& imageIndex,
    const MessageOptions& options){
  BlockPlan result;result.original=block;result.addMargin=options.addMargin;
  result.verbose=options.verbose;result.transcriptMode=options.transcriptMode;
  const auto& type=value::property(block.value(),"type");
  if(type=="text")result.kind=BlockKind::UserText;
  else if(type=="image"){
    result.kind=BlockKind::UserImage;result.imageIndex=imageIndex;
    result.addMargin=options.addMargin&&!options.isUserContinuation;
  }else if(type=="tool_result"){
    result.kind=BlockKind::GadgetResult;result.width=options.columns-5;
  }
  return result;
}
BlockPlan prepareAssistantBlock(const GadgetBlockRef& block,const std::string& thinkingId,
    const MessageOptions& options,const MessageImports& imports){
  BlockPlan result;result.original=block;result.addMargin=options.addMargin;
  result.verbose=options.verbose;result.transcriptMode=options.transcriptMode;
  result.shouldShowDot=options.shouldShowDot;result.shouldAnimate=options.shouldAnimate;
  result.width=options.width;result.thinkingBlockId=thinkingId;
  result.inProgressGadgetCallCount=options.inProgressGadgetUseIDs.size();
  need(imports.feature,"feature");
  if(imports.feature("CONNECTOR_TEXT")){
    need(imports.isConnectorTextBlock,"isConnectorTextBlock");
    if(imports.isConnectorTextBlock(block.value())){
      result.kind=BlockKind::AssistantText;
      result.replacement={{"type","text"},{"text",value::property(block.value(),"connector_text")}};
      return result;
    }
  }
  const auto& type=value::property(block.value(),"type");
  if(type=="tool_use")result.kind=BlockKind::AssistantGadgetUse;
  else if(type=="text")result.kind=BlockKind::AssistantText;
  else if(type=="redacted_thinking"){
    if(options.transcriptMode||options.verbose)result.kind=BlockKind::RedactedThinking;
  }else if(type=="thinking"){
    if(!options.transcriptMode&&!options.verbose&&!options.showThinkingInPlace)return result;
    result.kind=BlockKind::Thinking;
    if(options.showThinkingInPlace)result.transcriptMode=true;
    const bool last=!value::truthy(options.lastThinkingBlockId)||
      Json(thinkingId)==options.lastThinkingBlockId;
    result.hideInTranscript=options.transcriptMode&&!last&&!options.showThinkingInPlace;
  }else if(type=="server_tool_use"||type=="advisor_tool_result"){
    need(imports.isAdvisorBlock,"isAdvisorBlock");
    if(imports.isAdvisorBlock(block.value())){
      result.kind=BlockKind::Advisor;result.verbose=options.verbose||options.transcriptMode;
      return result;
    }
    need(imports.logError,"logError");
    imports.logError("Unable to render server Gadget block: "+type.get<std::string>());
  }else{
    need(imports.logError,"logError");
    imports.logError("Unable to render message type: "+
      (type.is_string()?type.get<std::string>():std::string("undefined")));
  }
  return result;
}
} // namespace batbox::interactive::message_view
