#include "application_cli/Adapter.hpp"
#include "application_cli/LocalOutput.hpp"
#include "messages/Context.hpp"
#include "npm_wrap_ansi/Internal.hpp"
#include "runtime/MessageSnapshot.hpp"
#include "runtime/Platform.hpp"
#include <algorithm>

namespace batbox::application_cli {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
void put(Json& result,const char* key,const Json& value){
  if(!value.is_discarded())result[key]=value;
}
bool commandTag(const std::string& text){
  return text.find("<local-command-stdout>")!=std::string::npos||
    text.find("<local-command-stderr>")!=std::string::npos;
}
std::string strip(const std::string& text){
  return value::fromUtf16(npm_wrap_ansi::stripAnsi(value::utf16CodeUnits(text)));
}
}
Json toSdkCompactMetadata(const Json& metadata){
  Json result=Json::object();
  put(result,"trigger",field(metadata,"trigger"));
  put(result,"pre_tokens",field(metadata,"preTokens"));
  const auto& segment=field(metadata,"preservedSegment");
  if(value::truthy(segment)){
    Json mapped=Json::object();
    put(mapped,"head_uuid",field(segment,"headUuid"));
    put(mapped,"anchor_uuid",field(segment,"anchorUuid"));
    put(mapped,"tail_uuid",field(segment,"tailUuid"));
    result["preserved_segment"]=std::move(mapped);
  }
  return result;
}
std::string localOutputText(const std::string& raw){
  auto text=strip(raw);
  for(const auto* tag:{"local-command-stdout","local-command-stderr"}){
    const auto open="<"+std::string(tag)+">",close="</"+std::string(tag)+">";
    const auto begin=text.find(open);
    if(begin==std::string::npos)continue;
    const auto end=text.find(close,begin+open.size());
    if(end==std::string::npos)continue;
    text.erase(end,close.size());text.erase(begin,open.size());
  }
  return value::trim(text);
}
Json localCommandOutputToSdkAssistantMessage(const std::string& raw,
    const Json& uuid,const std::string& flightId,messages::Services& messages){
  auto synthetic=messages::createAssistantMessage(Json{{"content",localOutputText(raw)}},messages);
  return Json{{"type","assistant"},{"message",field(synthetic,"message")},
    {"parent_tool_use_id",nullptr},{"session_id",flightId},{"uuid",uuid}};
}
void Adapter::onTurnRequested(const interactive::TurnRequest& request){
  if(stopped_||!busy_)return;
  localTurn_=!request.shouldQuery;
  localMessages_=localTurn_?request.newMessages:nullptr;
}
void Adapter::emitLocalMessages(){
  if(!localMessages_)return;
  const bool compacted=std::any_of(localMessages_->begin(),localMessages_->end(),
    [](const auto& ref){return ref&&messages::isCompactBoundaryMessage(*ref);});
  messages::Services fallback;
  fallback.randomUUID=inputs_.randomUUID;
  fallback.nowISO=[] {return platform::isoTimestamp();};
  auto& messages=inputs_.messageServices?*inputs_.messageServices:fallback;
  for(const auto& ref:*localMessages_){
    if(!ref)continue;
    const auto message=runtime::snapshotMessage(ref);
    const auto& type=field(message,"type");
    const auto& content=field(field(message,"message"),"content");
    if(type=="user"&&content.is_string()&&
        (commandTag(content.get<std::string>())||value::truthy(field(message,"isCompactSummary")))){
      Json body=field(message,"message");body["content"]=strip(content.get<std::string>());
      Json sdk={{"type","user"},{"message",std::move(body)},
        {"session_id",inputs_.flightId()},{"parent_tool_use_id",nullptr}};
      put(sdk,"uuid",field(message,"uuid"));put(sdk,"timestamp",field(message,"timestamp"));
      sdk["isReplay"]=!value::truthy(field(message,"isCompactSummary"));
      if(value::truthy(field(message,"isMeta")))sdk["isSynthetic"]=field(message,"isMeta");
      else if(message.contains("isVisibleInTranscriptOnly"))
        sdk["isSynthetic"]=message.at("isVisibleInTranscriptOnly");
      emit(std::move(sdk));
      if(!compacted&&content.get<std::string>().find("<local-command-stdout>")!=std::string::npos)
        localResultText_=localOutputText(content.get<std::string>());
    }
    const auto& systemContent=field(message,"content");
    if(type=="system"&&field(message,"subtype")=="local_command"&&
        systemContent.is_string()&&commandTag(systemContent.get<std::string>())){
      emit(localCommandOutputToSdkAssistantMessage(systemContent.get<std::string>(),
        field(message,"uuid"),inputs_.flightId(),messages));
      if(!compacted)localResultText_=localOutputText(systemContent.get<std::string>());
    }
    if(messages::isCompactBoundaryMessage(message)){
      Json sdk={{"type","system"},{"subtype","compact_boundary"},
        {"session_id",inputs_.flightId()}};
      put(sdk,"uuid",field(message,"uuid"));
      sdk["compact_metadata"]=toSdkCompactMetadata(field(message,"compactMetadata"));
      emit(std::move(sdk));
    }
  }
}
}
