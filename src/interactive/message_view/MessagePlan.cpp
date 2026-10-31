#include "interactive/message_view/MessagePlan.hpp"
#include <stdexcept>

namespace batbox::interactive::message_view {
namespace {
void need(const auto& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Message view import required: ")+name);
}
const Json& field(const Json& value,const char* key){return value::property(value,key);}
bool latest(const MessageRef& message,const Json& uuid){
  return field(*message,"uuid")==uuid;
}
}
MessagePlan prepareMessage(RenderItem item,MessageOptions options,
    const MessageImports& imports){
  need(item.message,"message");
  MessagePlan result(std::move(item),std::move(options));
  const Json& message=*result.item.message;
  const auto& type=field(message,"type");
  const auto& settings=result.options;
  if(type=="attachment"){result.kind=MessageKind::Attachment;return result;}
  if(type=="assistant"||type=="user"){
    if(type=="user"&&value::truthy(field(message,"isCompactSummary"))){
      result.kind=MessageKind::CompactSummary;return result;
    }
    result.kind=type=="assistant"?MessageKind::Assistant:MessageKind::User;
    const auto& content=field(field(message,"message"),"content");
    if(!content.is_array())throw std::invalid_argument("Normalized message content array required");
    std::size_t imagePosition=0;
    const auto& pasteIds=field(message,"imagePasteIds");
    for(std::size_t index=0;index<content.size();++index){
      GadgetBlockRef block{result.item.message,index};
      if(type=="assistant"){
        const auto uuid=field(message,"uuid").get<std::string>();
        result.blocks.push_back(prepareAssistantBlock(block,uuid+":"+
          std::to_string(index),settings,imports));
      }else{
        Json imageIndex=imagePosition;
        if(field(content[index],"type")=="image"){
          const Json id=pasteIds.is_array()&&imagePosition<pasteIds.size()?
            pasteIds[imagePosition]:Json(nullptr);
          ++imagePosition;imageIndex=id.is_null()?Json(imagePosition):id;
        }
        result.blocks.push_back(prepareUserBlock(block,imageIndex,settings));
      }
    }
    if(type=="user")result.expandShellOutput=latest(result.item.message,settings.latestBashOutputUUID);
    return result;
  }
  if(type=="system"){
    const auto& subtype=field(message,"subtype");
    if(subtype=="compact_boundary"){
      need(imports.fullscreen,"fullscreen");
      if(!imports.fullscreen())result.kind=MessageKind::CompactBoundary;
      return result;
    }
    if(subtype=="microcompact_boundary")return result;
    need(imports.feature,"feature");
    if(imports.feature("HISTORY_SNIP")){
      need(imports.isSnipBoundary,"isSnipBoundary");
      if(imports.isSnipBoundary(message)){result.kind=MessageKind::SnipBoundary;return result;}
      need(imports.isSnipMarker,"isSnipMarker");
      if(imports.isSnipMarker(message))return result;
    }
    if(subtype=="local_command"){
      result.kind=MessageKind::LocalCommand;
      BlockPlan block;block.kind=BlockKind::UserText;
      block.replacement={{"type","text"},{"text",field(message,"content")}};
      block.addMargin=settings.addMargin;block.verbose=settings.verbose;
      block.transcriptMode=settings.transcriptMode;
      result.blocks.push_back(std::move(block));
      return result;
    }
    result.kind=MessageKind::System;return result;
  }
  if(type=="grouped_tool_use")result.kind=MessageKind::GroupedGadgets;
  else if(type=="collapsed_read_search"){
    result.kind=MessageKind::CollapsedReadSearch;
  }
  return result;
}
bool hasThinkingContent(const Json& message){
  if(field(message,"type")!="assistant"||!value::truthy(field(message,"message")))return false;
  const auto& content=field(field(message,"message"),"content");
  if(!content.is_array())throw std::invalid_argument("Assistant content array required");
  for(const auto& block:content){
    const auto& type=field(block,"type");
    if(type=="thinking"||type=="redacted_thinking")return true;
  }
  return false;
}
bool sameThinkingMarker(const Json& previous,const Json& next){
  return (previous.is_discarded()&&next.is_discarded())||previous==next;
}
bool sameMessageView(const MessagePlan& previous,const MessagePlan& next){
  const auto& a=*previous.item.message;const auto& b=*next.item.message;
  const auto& p=previous.options;const auto& n=next.options;
  if(field(a,"uuid")!=field(b,"uuid"))return false;
  if(!sameThinkingMarker(p.lastThinkingBlockId,n.lastThinkingBlockId)&&
      hasThinkingContent(b))return false;
  if(p.verbose!=n.verbose)return false;
  if(p.showThinkingInPlace!=n.showThinkingInPlace)return false;
  if(latest(previous.item.message,p.latestBashOutputUUID)!=
      latest(next.item.message,n.latestBashOutputUUID))return false;
  if(p.transcriptMode!=n.transcriptMode)return false;
  if(p.containerWidth!=n.containerWidth)return false;
  return p.isStatic&&n.isStatic;
}
} // namespace batbox::interactive::message_view
