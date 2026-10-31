#include "interactive/message_list_view/Surface.hpp"
#include <stdexcept>

namespace batbox::interactive::message_list_view {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
}
bool SurfaceState::thinkingVisible(const std::shared_ptr<const StreamingThinking>& thinking,
    double nowMs){
  // Recompute when the streaming-thinking object changes.
  if(thinking==previousThinking_)return thinkingVisible_;
  previousThinking_=thinking;thinkingVisible_=false;
  if(thinking){
    if(thinking->isStreaming)thinkingVisible_=true;
    else if(thinking->streamingEndedAt&&*thinking->streamingEndedAt!=0)
      thinkingVisible_=nowMs-*thinking->streamingEndedAt<30000;
  }
  return thinkingVisible_;
}
void SurfaceState::toggleExpanded(const Json& message){
  const auto key=expandKey(message);
  if(!expandedKeys_.erase(key))expandedKeys_.insert(key);
}
bool SurfaceState::expanded(const Json& message)const{
  return !expandedKeys_.empty()&&expandedKeys_.contains(expandKey(message));
}
void SurfaceState::committed(bool progressEnabled,bool hasGadgetsInProgress,
    const std::function<void(std::optional<ProgressState>)>& progress){
  const auto next=progressEnabled?std::optional<ProgressState>(hasGadgetsInProgress?
    ProgressState::Indeterminate:ProgressState::Completed):std::nullopt;
  if(next==previousProgress_)return;
  previousProgress_=next;
  if(!progress)throw std::invalid_argument("Terminal progress callback required");
  progress(next);
}
void SurfaceState::unmounted(const std::function<void(std::optional<ProgressState>)>& progress){
  if(!progress)throw std::invalid_argument("Terminal progress callback required");
  progress(std::nullopt);
}
Json lastThinkingBlockId(const MessageRefs& messages,bool hidePast,bool streamingVisible){
  if(!hidePast)return nullptr;
  if(streamingVisible)return "streaming";
  for(auto message=messages.rbegin();message!=messages.rend();++message){
    if(!*message)continue;
    const auto& content=field(field(**message,"message"),"content");
    if(field(**message,"type")=="assistant"){
      for(std::size_t i=content.size();i>0;--i)
        if(field(content[i-1],"type")=="thinking")
          return field(**message,"uuid").get<std::string>()+":"+std::to_string(i-1);
    }else if(field(**message,"type")=="user"){
      bool result=false;
      for(const auto& block:content)if(field(block,"type")=="tool_result"){result=true;break;}
      if(!result)return "no-thinking";
    }
  }
  return nullptr;
}
Json latestBashOutputUUID(const MessageRefs& messages){
  for(auto message=messages.rbegin();message!=messages.rend();++message){
    if(!*message||field(**message,"type")!="user")continue;
    for(const auto& block:field(field(**message,"message"),"content")){
      if(field(block,"type")!="text")continue;
      const auto text=field(block,"text").get<std::string>();
      if(text.starts_with("<bash-stdout")||text.starts_with("<bash-stderr"))return field(**message,"uuid");
    }
  }
  return nullptr;
}
bool isItemClickable(const Json& message,const MessageLookups& lookups,
    const gadgets::Gadgets& gadgets,const std::function<bool(const Json&)>& isAdvisorBlock){
  const auto& type=field(message,"type");
  if(type=="collapsed_read_search")return true;
  const auto& content=field(field(message,"message"),"content");
  const Json first=content.is_array()&&!content.empty()?content[0]:Json(nullptr);
  if(type=="assistant"){
    if(first.is_null())return false;
    if(!isAdvisorBlock)throw std::invalid_argument("Advisor block predicate required");
    return isAdvisorBlock(first)&&field(first,"type")=="advisor_tool_result"&&
      field(field(first,"content"),"type")=="advisor_result";
  }
  if(type!="user")return false;
  if(field(first,"type")!="tool_result"||value::truthy(field(first,"is_error"))||
      !value::truthy(field(message,"toolUseResult")))return false;
  const auto& id=field(first,"tool_use_id");
  const auto found=id.is_string()?lookups.gadgetUseByGadgetUseID.find(id.get<std::string>()):lookups.gadgetUseByGadgetUseID.end();
  if(found==lookups.gadgetUseByGadgetUseID.end())return false;
  const auto& name=field(found->second.value(),"name");
  const auto* gadget=name.is_string()?gadgets::findGadgetByName(gadgets,name.get<std::string>()):nullptr;
  return gadget&&gadget->isResultTruncated?gadget->isResultTruncated(message.at("toolUseResult")):false;
}
} // namespace batbox::interactive::message_list_view
