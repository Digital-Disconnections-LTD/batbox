#include "query_engine/NormalizeMessage.hpp"
#include "utils/envUtils.hpp"

namespace batbox::query_engine {
namespace {
const Json& field(const Json& value,std::string_view name){
  static const Json missing(Json::value_t::discarded);
  if(!value.is_object())return missing;
  const auto found=value.find(std::string(name));return found==value.end()?missing:*found;
}
void put(Json& out,const std::string& name,const Json& value){if(!value.is_discarded())out[name]=value;}
struct Cursor {
  MessageRef message;
  std::shared_ptr<NormalizeState> state;
  std::shared_ptr<NormalizeServices> services;
  MessagesRef normalized;
  std::size_t index=0;
  bool initialized=false,done=false,nested=false,assistantMode=false;
  Json assistant(const MessageRef& m,const Json& parent){
    Json sdk{{"type","assistant"}};put(sdk,"message",field(*m,"message"));
    put(sdk,"parent_tool_use_id",parent);sdk["session_id"]=services->getFlightId();
    put(sdk,"uuid",field(*m,"uuid"));put(sdk,"error",field(*m,"error"));return sdk;
  }
  Json user(const MessageRef& m,const Json& parent){
    Json sdk{{"type","user"}};put(sdk,"message",field(*m,"message"));
    put(sdk,"parent_tool_use_id",parent);sdk["session_id"]=services->getFlightId();
    put(sdk,"uuid",field(*m,"uuid"));put(sdk,"timestamp",field(*m,"timestamp"));
    const auto& meta=field(*m,"isMeta");put(sdk,"isSynthetic",value::truthy(meta)?meta:field(*m,"isVisibleInTranscriptOnly"));
    const auto& mcp=field(*m,"mcpMeta");
    if(value::truthy(mcp)){
      Json result=Json::object();put(result,"content",field(*m,"toolUseResult"));
      for(const auto& key:value::objectKeys(mcp))result[key]=mcp.at(key);
      sdk["tool_use_result"]=std::move(result);
    }else put(sdk,"tool_use_result",field(*m,"toolUseResult"));
    return sdk;
  }
  std::optional<Json> shellProgress(){
    done=true;
    const auto remote=services->env("BATBOX_CODE_REMOTE");
    if(!isEnvTruthy(remote?Json(*remote):Json(nullptr))){
      const auto container=services->env("BATBOX_CODE_CONTAINER_ID");
      if(!container||container->empty())return {};
    }
    const auto key=field(*message,"parentToolUseID").get<std::string>();
    const double now=services->now();
    auto found=state->gadgetProgressLastSentTime.end();
    for(auto it=state->gadgetProgressLastSentTime.begin();it!=state->gadgetProgressLastSentTime.end();++it)
      if(it->first==key){found=it;break;}
    const double last=found==state->gadgetProgressLastSentTime.end()?0:found->second;
    if(!(now-last>=30000))return {};
    if(state->gadgetProgressLastSentTime.size()>=100){
      if(found==state->gadgetProgressLastSentTime.begin())found=state->gadgetProgressLastSentTime.end();
      state->gadgetProgressLastSentTime.pop_front();
    }
    if(found==state->gadgetProgressLastSentTime.end())state->gadgetProgressLastSentTime.emplace_back(key,now);
    else found->second=now;
    const auto& data=field(*message,"data");
    Json sdk{{"type","tool_progress"}};put(sdk,"tool_use_id",field(*message,"toolUseID"));
    sdk["tool_name"]=field(data,"type")=="bash_progress"?"Bash":"PowerShell";
    put(sdk,"parent_tool_use_id",field(*message,"parentToolUseID"));
    put(sdk,"elapsed_time_seconds",field(data,"elapsedTimeSeconds"));put(sdk,"task_id",field(data,"taskId"));
    sdk["session_id"]=services->getFlightId();put(sdk,"uuid",field(*message,"uuid"));return sdk;
  }
  std::optional<Json> next(){
    if(done)return {};
    if(!initialized){
      initialized=true;const auto& type=field(*message,"type");
      assistantMode=type=="assistant";
      MessageRef input=message;
      if(type=="progress"){
        const auto& data=field(*message,"data");const auto& kind=field(data,"type");
        if(kind=="bash_progress"||kind=="powershell_progress")return shellProgress();
        if(kind!="agent_progress"&&kind!="skill_progress"){done=true;return {};}
        nested=true;input=std::make_shared<Json>(field(data,"message"));
      }else if(type!="assistant"&&type!="user"){done=true;return {};}
      normalized=services->normalizeMessages(std::make_shared<Messages>(Messages{input}));
    }
    while(index<normalized->size()){
      const auto current=normalized->at(index++);
      const auto parent=nested?field(*message,"parentToolUseID"):Json(nullptr);
      const auto type=nested?field(*current,"type"):Json(assistantMode?"assistant":"user");
      if(type=="assistant"){
        if(!services->isNotEmptyMessage(current))continue;
        return assistant(current,parent);
      }
      if(type=="user")return user(current,parent);
    }
    done=true;return {};
  }
};
}
SdkCursor normalizeMessage(MessageRef message,std::shared_ptr<NormalizeState> state,
    std::shared_ptr<NormalizeServices> services){
  auto cursor=std::make_shared<Cursor>();cursor->message=std::move(message);
  cursor->state=std::move(state);cursor->services=std::move(services);
  return SdkCursor{[cursor]{return cursor->next();}};
}
}
