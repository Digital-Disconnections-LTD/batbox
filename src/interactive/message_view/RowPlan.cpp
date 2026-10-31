#include "interactive/message_view/RowPlan.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::interactive::message_view {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
bool anyProgress(const Json& group,const GadgetIDs& ids){
  for(const auto& id:getGadgetUseIdsFromCollapsedGroup(group))if(ids.contains(id))return true;
  return false;
}
MessageRef display(const RenderItem& item){
  const auto& type=field(*item.message,"type");
  if(type=="grouped_tool_use"){
    if(!item.displayMessage)throw std::invalid_argument("Grouped display message required");
    return item.displayMessage;
  }
  if(type=="collapsed_read_search"){
    if(!item.displayItem)throw std::invalid_argument("Collapsed display item required");
    const auto& first=*item.displayItem;
    return field(*first.message,"type")=="grouped_tool_use"?display(first):first.message;
  }
  return item.message;
}
bool latest(const RowRequest& request){
  return field(*request.item.message,"uuid")==request.options.latestBashOutputUUID;
}
}
MessageRefs progressFromLookup(const Json& message,const MessageLookups& lookups){
  const auto id=getGadgetUseID(message);
  if(!id||id->empty())return {};
  const auto found=lookups.progressMessagesByGadgetUseID.find(*id);
  return found==lookups.progressMessagesByGadgetUseID.end()?MessageRefs{}:found->second;
}
GadgetIDs siblingsFromLookup(const Json& message,const MessageLookups& lookups){
  const auto id=getGadgetUseID(message);
  if(!id||id->empty())return {};
  const auto found=lookups.siblingGadgetUseIDs.find(*id);
  if(found==lookups.siblingGadgetUseIDs.end()||!found->second)return {};
  return {found->second->begin(),found->second->end()};
}
RowPlan prepareRow(RowRequest request,const MessageImports& imports){
  if(!request.item.message||!request.lookups)throw std::invalid_argument("Row message and lookups required");
  const auto message=materializeRenderItem(request.item);
  const auto& type=field(message,"type");
  const bool grouped=type=="grouped_tool_use",collapsed=type=="collapsed_read_search";
  auto options=request.options;
  options.width=Json(Json::value_t::discarded);
  options.style=Json(Json::value_t::discarded);
  options.transcriptMode=request.screen=="transcript";
  options.isActiveCollapsedGroup=collapsed&&
    (anyProgress(message,options.inProgressGadgetUseIDs)||
      (request.isLoading&&!request.hasContentAfter));
  auto visible=display(request.item);
  options.progressMessages=grouped||collapsed?MessageRefs{}:
    progressFromLookup(message,*request.lookups);
  const auto siblings=grouped||collapsed?GadgetIDs{}:siblingsFromLookup(message,*request.lookups);
  options.isStatic=shouldRenderStatically(message,request.streamingGadgetUseIDs,
    options.inProgressGadgetUseIDs,siblings,request.screen,*request.lookups);
  options.shouldAnimate=false;
  if(request.canAnimate){
    if(grouped){
      for(const auto& member:request.item.members){
        const auto& content=field(field(*member,"message"),"content");
        if(!content.is_array()||content.empty())continue;
        if(field(content[0],"type")=="tool_use"&&
            options.inProgressGadgetUseIDs.contains(field(content[0],"id").get<std::string>())){
          options.shouldAnimate=true;break;
        }
      }
    }else if(collapsed)options.shouldAnimate=anyProgress(message,options.inProgressGadgetUseIDs);
    else{
      const auto id=getGadgetUseID(message);
      options.shouldAnimate=!id||id->empty()||options.inProgressGadgetUseIDs.contains(*id);
    }
  }
  bool text=false;
  const auto& inner=field(*visible,"message");
  const auto& content=field(inner,"content");
  if(options.transcriptMode&&field(*visible,"type")=="assistant"&&content.is_array())
    for(const auto& block:content)if(field(block,"type")=="text"){text=true;break;}
  const bool metadata=text&&(value::truthy(field(*visible,"timestamp"))||value::truthy(field(inner,"model")));
  options.addMargin=!metadata;
  options.containerWidth=metadata?std::nullopt:std::optional<int>(options.columns);
  options.shouldShowDot=true;
  auto plan=prepareMessage(request.item,std::move(options),imports);
  return {std::move(request),std::move(visible),std::move(plan),metadata};
}
bool sameRowView(const RowRequest& previous,const RowRequest& next){
  if(previous.item.message!=next.item.message)return false;
  if(previous.screen!=next.screen)return false;
  if(previous.options.verbose!=next.options.verbose)return false;
  if(previous.options.showThinkingInPlace!=next.options.showThinkingInPlace)return false;
  if(field(*previous.item.message,"type")=="collapsed_read_search"&&next.screen!="transcript")return false;
  if(previous.options.columns!=next.options.columns)return false;
  if(latest(previous)!=latest(next))return false;
  if(!sameThinkingMarker(previous.options.lastThinkingBlockId,next.options.lastThinkingBlockId)&&
      hasThinkingContent(*next.item.message))return false;
  if(!previous.lookups)throw std::invalid_argument("Previous row lookups required");
  const auto message=materializeRenderItem(previous.item);
  return !isMessageStreaming(message,previous.streamingGadgetUseIDs)&&
    allGadgetsResolved(message,previous.lookups->resolvedGadgetUseIDs);
}
} // namespace batbox::interactive::message_view
