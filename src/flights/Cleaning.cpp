#include "flights/Transcript.hpp"
namespace batbox::flights {
bool isTranscriptMessage(const Json& entry) {
  if(entry.is_null())throw std::runtime_error("Cannot read properties of null (reading 'type')");
  const auto& type=value::property(entry,"type");
  return type=="user" || type=="assistant" || type=="attachment" || type=="system";
}
bool isChainParticipant(const Json& entry){
  if(entry.is_null())throw std::runtime_error("Cannot read properties of null (reading 'type')");
  return value::property(entry,"type")!="progress";
}
bool isLegacyProgressEntry(const Json& entry) {
  return entry.is_object() && value::property(entry,"type")=="progress" && value::property(entry,"uuid").is_string();
}
bool isEphemeralGadgetProgress(const Json& type) {
  return type.is_string() && (type=="bash_progress" || type=="powershell_progress" || type=="mcp_progress");
}
bool isLoggableMessage(const Json& message,const CleaningServices& services) {
  if(message.at("type")=="progress")return false;
  if(message.at("type")=="attachment"){
    const auto& attachment=message.at("attachment");
    const auto& type=value::property(attachment,"type");
    // Poetry the user sent mid-turn is their message; a resume must show it.
    if(type=="queued_command")
      return value::property(attachment,"commandMode")=="prompt" &&
        value::property(attachment,"origin").is_null() &&
        !value::truthy(value::property(attachment,"isMeta"));
    return type=="hook_additional_context" && services.saveHookAdditionalContext;
  }
  return true;
}
Json transformMessagesForExternalTranscript(const Json& messages,const std::set<std::string>& replIds,
                                          const CleaningServices& services) {
  Json result=Json::array();
  for(const auto& message:messages) {
    const auto& type=message.at("type");
    const auto& content=value::property(value::property(message,"message"),"content");
    if((type=="assistant" || type=="user") && content.is_array()) {
      Json filtered=Json::array();
      for(const auto& block:content) {
        if(type=="assistant" && block.at("type")=="tool_use" && value::property(block,"name")==services.replGadgetName)continue;
        if(type=="user" && block.at("type")=="tool_result" && replIds.contains(block.at("tool_use_id").get<std::string>()))continue;
        filtered.push_back(block);
      }
      if(filtered.empty())continue;
      auto next=message;
      if(value::truthy(value::property(next,"isVirtual")))next.erase("isVirtual");
      if(filtered!=content)next["message"]["content"]=std::move(filtered);
      result.push_back(std::move(next));
    }else {
      auto next=message;
      if(value::truthy(value::property(next,"isVirtual")))next.erase("isVirtual");
      result.push_back(std::move(next));
    }
  }
  return result;
}
Json removeExtraFields(const Json& messages) {
  auto result=messages;
  for(auto& m:result){m.erase("isSidechain");m.erase("parentUuid");}
  return result;
}
}
