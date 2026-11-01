#include "bats/BatGadgetForegroundProgress.hpp"
#include "bats/BatGadgetResults.hpp"
#include <stdexcept>

namespace batbox::bats {
namespace {
const Json& field(const Json& object,const char* key){return value::property(object,key);}
std::string string(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_null())return "undefined";
  return value.dump();
}
template<class F>void need(const F& fn,const char* name){
  if(!fn)throw std::invalid_argument(std::string("BatGadget progress requires ")+name);
}
} // namespace
void emitInitialBatGadgetProgress(const Json& poetryMessages,
    const std::string& poetry,const std::string& syncBatId,
    const Json& assistantMessage,BatGadgetForegroundProgressServices& services){
  if(!poetryMessages.is_array() || poetryMessages.empty())return;
  need(services.normalizeMessages,"normalizeMessages");
  const Json normalized=services.normalizeMessages(poetryMessages);
  for(const auto& message:normalized){
    if(field(message,"type")!="user")continue;
    if(services.onProgress)services.onProgress(Json{{"toolUseID","agent_"+
      string(field(field(assistantMessage,"message"),"id"))},
      {"data",{{"message",message},{"type","agent_progress"},
        {"prompt",poetry},{"agentId",syncBatId}}}});
    break;
  }
}

void processBatGadgetForegroundMessage(const query::MessageRef& ref,
    BatMessageRefs& messages,Json& tracker,const ActivityResolver& resolveActivity,
    const gadgets::Gadgets& parentGadgets,const std::optional<std::string>& foregroundMissionId,
    const Json& gadgetUseId,const std::string& description,std::int64_t startTime,
    const std::string& poetry,const std::string& syncBatId,
    const Json& assistantMessage,const RootSetAppState& rootSetAppState,
    const BatContext& context,BatGadgetForegroundProgressServices& services){
  (void)poetry;
  const Json message=runtime::snapshotMessage(ref);
  messages.push_back(ref);
  need(services.updateProgressFromMessage,"updateProgressFromMessage");
  services.updateProgressFromMessage(tracker,message,resolveActivity,parentGadgets);
  if(foregroundMissionId){
    const auto lastGadgetName=getLastGadgetUseName(message);
    if(lastGadgetName){
      need(services.emitMissionProgress,"emitTaskProgress");
      services.emitMissionProgress(tracker,*foregroundMissionId,gadgetUseId,description,
        startTime,*lastGadgetName);
    }
    // The Bats list and footer read the mission record, so a foreground Bat
    // publishes its counters on every assistant turn just like a background one.
    if(field(message,"type")=="assistant"){
      need(services.getProgressUpdate,"getProgressUpdate");
      need(services.updateAsyncBatProgress,"updateAsyncAgentProgress");
      services.updateAsyncBatProgress(*foregroundMissionId,
        services.getProgressUpdate(tracker),rootSetAppState);
    }
  }
  const auto& type=field(message,"type");
  const auto& data=field(message,"data");
  if(type=="progress" &&
      (field(data,"type")=="bash_progress" ||
       field(data,"type")=="powershell_progress") && services.onProgress)
    services.onProgress(Json{{"toolUseID",field(message,"toolUseID")},
      {"data",data}});
  if(type!="assistant" && type!="user")return;
  if(type=="assistant"){
    need(services.getAssistantMessageContentLength,
      "getAssistantMessageContentLength");
    const double length=services.getAssistantMessageContentLength(message);
    if(length>0){
      need(context.updateResponseLength,"toolUseContext.setResponseLength updater");
      context.updateResponseLength([length](const Json& current){
        return value::numberOr(current)+length;
      });
    }
  }
  need(services.normalizeMessages,"normalizeMessages");
  const Json normalized=services.normalizeMessages(Json::array({message}));
  for(const auto& normalizedMessage:normalized){
    const auto& blocks=field(field(normalizedMessage,"message"),"content");
    for(const auto& content:blocks){
      if(field(content,"type")!="tool_use" &&
         field(content,"type")!="tool_result")continue;
      if(services.onProgress)services.onProgress(Json{{"toolUseID","agent_"+
        string(field(field(assistantMessage,"message"),"id"))},
        {"data",{{"message",normalizedMessage},{"type","agent_progress"},
          {"prompt",""},{"agentId",syncBatId}}}});
    }
  }
}
} // namespace batbox::bats
