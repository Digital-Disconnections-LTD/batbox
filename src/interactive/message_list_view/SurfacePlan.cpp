#include "interactive/message_list_view/Surface.hpp"
#include <stdexcept>

namespace batbox::interactive::message_list_view {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
}
SurfacePlan prepareSurface(std::shared_ptr<const PreparedMessageList> prepared,
    SurfaceOptions options,SurfaceState& state,double nowMs,
    const message_view::MessageImports& imports,const MessageDecisionImports& decisions){
  if(!prepared||!prepared->renderable)throw std::invalid_argument("Prepared messages required");
  SurfacePlan result;result.prepared=std::move(prepared);result.options=std::move(options);
  result.messageImports=imports;
  const auto& o=result.options;const auto& messages=*result.prepared;
  const bool thinkingVisible=state.thinkingVisible(o.streamingThinking,nowMs);
  result.lastThinkingBlockId=lastThinkingBlockId(messages.normalizedMessages,o.hidePastThinking,thinkingVisible);
  result.latestBashOutputUUID=latestBashOutputUUID(messages.normalizedMessages);
  std::ptrdiff_t divider=-1,selected=-1;
  const auto prefix=o.unseenDivider?value::slice(o.unseenDivider->firstUnseenUuid,0,24):"";
  for(std::size_t index=0;index<messages.renderable->size();++index){
    const auto& item=(*messages.renderable)[index];
    const auto& id=field(*item.message,"uuid").get_ref<const std::string&>();
    if(o.unseenDivider&&divider<0&&value::slice(id,0,24)==prefix)divider=index;
    if(o.cursor&&selected<0&&id==o.cursor->uuid)selected=index;
  }
  // Only collapsed read/search rows look ahead, and only grouped items differ
  // from their root message once materialized; build the list on demand.
  MessageRefs materialized;
  const auto materialize=[&]()->const MessageRefs&{
    if(materialized.empty()&&!messages.renderable->empty()){
      materialized.reserve(messages.renderable->size());
      for(const auto& item:*messages.renderable)
        materialized.push_back(item.members.empty()&&!item.collapsedItems?item.message:
          std::make_shared<Json>(materializeRenderItem(item)));
    }
    return materialized;
  };
  const bool canAnimate=(!o.gadgetUiContinueAnimation||*o.gadgetUiContinueAnimation)&&
    o.permissionQueueSize==0&&!o.messageSelectorVisible;
  auto lookups=std::shared_ptr<const MessageLookups>(result.prepared,&result.prepared->lookups);
  for(std::size_t index=0;index<messages.renderable->size();++index){
    const auto& item=(*messages.renderable)[index];
    const auto& type=field(*item.message,"type");
    const bool isSelected=static_cast<std::ptrdiff_t>(index)==selected;
    message_view::RowRequest request{item,{},lookups,o.streamingGadgetUseIDs,o.screen,canAnimate,o.isLoading,false};
    request.options.columns=o.columns;
    request.options.showThinkingInPlace=o.thinkingInPlace;
    request.options.verbose=o.verbose||state.expanded(*item.message)||
      (o.cursor&&o.cursor->expanded&&isSelected);
    request.options.inProgressGadgetUseIDs=o.inProgressGadgetUseIDs;
    request.options.lastThinkingBlockId=result.lastThinkingBlockId;
    request.options.latestBashOutputUUID=result.latestBashOutputUUID;
    request.options.isUserContinuation=type=="user"&&index>0&&
      field(*(*messages.renderable)[index-1].message,"type")=="user";
    request.hasContentAfter=type=="collapsed_read_search"&&
      ((o.streamingText&&!o.streamingText->empty())||
        hasContentAfterIndex(materialize(),index,o.streamingGadgetUseIDs,decisions));
    SurfaceRow row{field(*item.message,"uuid").get<std::string>()+"-"+o.conversationId,
      std::move(request),isSelected,{}};
    if(o.unseenDivider&&static_cast<std::ptrdiff_t>(index)==divider)row.unseenCount=o.unseenDivider->count;
    result.rows.push_back(std::move(row));
  }
  result.showLogo=!o.hideLogo&&!(o.renderRange&&o.renderRange->first>0);
  result.showTruncated=messages.hasTruncatedMessages;
  result.showAll=o.screen=="transcript"&&o.showAllInTranscript&&
    messages.hiddenMessageCount>0&&!o.disableRenderCap;
  result.showStreamingText=o.streamingText&&!o.streamingText->empty()&&!o.briefOnly;
  result.showStreamingThinking=thinkingVisible&&o.streamingThinking&&!o.briefOnly;
  if(o.thinkingInPlace){
    if(result.showStreamingThinking&&!o.streamingThinking->isStreaming){
      for(const auto& message:messages.normalizedMessages){
        const auto& content=field(field(*message,"message"),"content");
        if(!content.is_array())continue;
        for(const auto& block:content)
          if(field(block,"type")=="thinking"&&
              field(block,"thinking")==o.streamingThinking->thinking)
            result.showStreamingThinking=false;
      }
    }
    const auto addTransient=[&](SurfaceRow::Kind kind,const char* name){
      message_view::RowRequest request{RenderItem{MessageRef{}},{},{},{}};
      SurfaceRow row{o.conversationId+name,std::move(request),false,{}};
      row.kind=kind;result.rows.push_back(std::move(row));
    };
    if(result.showStreamingThinking)
      addTransient(SurfaceRow::Kind::StreamingThinking,"-live-thinking");
    if(result.showStreamingText)
      addTransient(SurfaceRow::Kind::StreamingText,"-live-response");
  }
  return result;
}
} // namespace batbox::interactive::message_list_view
