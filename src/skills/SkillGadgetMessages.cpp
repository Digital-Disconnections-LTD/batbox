#include "skills/SkillGadgetExecution.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::skills {
std::optional<std::string> getSkillGadgetUseIDFromParentMessage(
  const Json& parentMessage,const std::string& gadgetName){
  const auto& content=value::property(value::property(parentMessage,"message"),"content");
  if(!content.is_array())return std::nullopt;
  for(const auto& block:content){
    if(value::property(block,"type")=="tool_use"&&
       value::property(block,"name")==gadgetName){
      const auto& id=value::property(block,"id");
      if(id.is_string())return id.get<std::string>();
      return std::nullopt;
    }
  }
  return std::nullopt;
}
std::vector<Json> tagSkillMessagesWithGadgetUseID(
  const std::vector<Json>& messages,const std::optional<std::string>& gadgetUseID){
  if(!gadgetUseID||gadgetUseID->empty())return messages;
  std::vector<Json> tagged;
  for(auto message:messages){
    if(value::property(message,"type")=="user")message["sourceToolUseID"]=*gadgetUseID;
    tagged.push_back(std::move(message));
  }
  return tagged;
}
std::vector<Json> filterSkillProcessedMessages(const std::vector<Json>& messages,
  const std::string& commandMessageTag){
  std::vector<Json> filtered;
  for(const auto& message:messages){
    if(value::property(message,"type")=="progress")continue;
    if(value::property(message,"type")=="user"&&message.contains("message")){
      const auto& content=value::property(value::property(message,"message"),"content");
      if(content.is_string()&&content.get<std::string>().find(
          "<"+commandMessageTag+">")!=std::string::npos)continue;
    }
    filtered.push_back(message);
  }
  return filtered;
}
Json mapSkillGadgetResult(const Json& result,const std::string& gadgetUseID){
  const auto name=value::property(result,"commandName").get<std::string>();
  std::string content;
  if(value::property(result,"status")=="forked")
    content="Skill \""+name+"\" completed (forked execution).\n\nResult:\n"+
      value::property(result,"result").get<std::string>();
  else content="Launching skill: "+name;
  return {{"type","tool_result"},{"tool_use_id",gadgetUseID},{"content",content}};
}
}
