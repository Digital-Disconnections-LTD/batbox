#include "bats/BatGadgetPresentation.hpp"
#include <stdexcept>
#include <unordered_set>

namespace batbox::bats {
namespace {
std::string interpolated(const Json& object,const char* key) {
  if (!object.is_object() || !object.contains(key)) return "undefined";
  const auto& value=object.at(key);
  if (value.is_string()) return value.get<std::string>();
  if (value.is_null()) return "null";
  if (value.is_boolean()) return value.get<bool>()?"true":"false";
  if (value.is_number()) return value.dump();
  if (value.is_array()) {
    std::string result;
    for (std::size_t i=0;i<value.size();++i) {
      if (i) result+=",";
      if (!value[i].is_null()) {
        if (value[i].is_string()) result+=value[i].get<std::string>();
        else result+=value[i].dump();
      }
    }
    return result;
  }
  return "[object Object]";
}
Json contentText(const std::string& value) {
  return Json{{"type","text"},{"text",value}};
}
Json resultBlock(const std::string& id,const Json& content) {
  return Json{{"tool_use_id",id},{"type","tool_result"},{"content",content}};
}
bool hasStatus(const Json& data,const char* status) {
  return data.is_object() && data.contains("status") &&
    data.at("status").is_string() && data.at("status")==status;
}
} // namespace

Json mapBatGadgetResult(const Json& data,const std::string& gadgetUseId,
    const std::string& fileReadGadgetName,const std::string& bashGadgetName) {
  if (hasStatus(data,"teammate_spawned")) {
    const std::string text="Spawned successfully.\nagent_id: "+
      interpolated(data,"teammate_id")+"\nname: "+interpolated(data,"name")+
      "\nteam_name: "+interpolated(data,"team_name")+
      "\nThe agent is now running and will receive instructions via mailbox.";
    return resultBlock(gadgetUseId,Json::array({contentText(text)}));
  }
  if (hasStatus(data,"remote_launched")) {
    const std::string text="Remote agent launched in CCR.\ntaskId: "+
      interpolated(data,"taskId")+"\nsession_url: "+
      interpolated(data,"sessionUrl")+"\noutput_file: "+
      interpolated(data,"outputFile")+
      "\nThe agent is running remotely. You will be notified automatically when it completes."
      "\nBriefly tell the user what you launched and end your response.";
    return resultBlock(gadgetUseId,Json::array({contentText(text)}));
  }
  if (hasStatus(data,"async_launched")) {
    const std::string prefix="Async agent launched successfully.\nagentId: "+
      interpolated(data,"agentId")+
      " (internal ID - do not mention to user. Use SendMessage with to: '"+
      interpolated(data,"agentId")+
      "' to continue this agent.)\nThe agent is working in the background. You will be notified automatically when it completes.";
    const bool readable=data.contains("canReadOutputFile") &&
      value::truthy(data.at("canReadOutputFile"));
    const std::string instructions=readable?
      "Do not duplicate this agent's work — avoid working with the same files or topics it is using. Work on non-overlapping tasks, or briefly tell the user what you launched and end your response.\noutput_file: "+
      interpolated(data,"outputFile")+
      "\nIf asked, you can check progress before completion by using "+
      fileReadGadgetName+" or "+bashGadgetName+" tail on the output file.":
      "Briefly tell the user what you launched and end your response. Do not generate any other text — agent results will arrive in a subsequent message.";
    return resultBlock(gadgetUseId,Json::array({contentText(prefix+"\n"+instructions)}));
  }
  if (hasStatus(data,"completed")) {
    const bool hasWorktree=data.contains("worktreePath") &&
      value::truthy(data.at("worktreePath"));
    const std::string worktree=hasWorktree?
      "\nworktreePath: "+interpolated(data,"worktreePath")+
      "\nworktreeBranch: "+interpolated(data,"worktreeBranch"):"";
    Json content=data.at("content");
    if (content.empty()) content=Json::array({contentText("(Subagent completed but returned no output.)")});
    const bool oneShot=data.contains("agentType") && value::truthy(data.at("agentType")) &&
      data.at("agentType").is_string() &&
      (data.at("agentType")=="Explore" || data.at("agentType")=="Plan");
    if (oneShot && !hasWorktree) return resultBlock(gadgetUseId,content);
    content.push_back(contentText("agentId: "+interpolated(data,"agentId")+
      " (use SendMessage with to: '"+interpolated(data,"agentId")+
      "' to continue this agent)"+worktree+"\n<usage>total_tokens: "+
      interpolated(data,"totalTokens")+"\ntool_uses: "+
      interpolated(data,"totalToolUseCount")+"\nduration_ms: "+
      interpolated(data,"totalDurationMs")+"</usage>"));
    return resultBlock(gadgetUseId,content);
  }
  throw std::runtime_error("Unexpected agent tool result status: "+
    interpolated(data,"status"));
}

std::string batGadgetAutoClassifierInput(const Json& input) {
  std::string tags;
  if (input.contains("subagent_type")) tags=interpolated(input,"subagent_type");
  if (input.contains("mode") && value::truthy(input.at("mode"))) {
    if (!tags.empty()) tags+=", ";
    tags+="mode="+interpolated(input,"mode");
  }
  return (tags.empty()?": ":"("+tags+"): ")+interpolated(input,"prompt");
}

Json batGadgetActivityDescription(const Json& input) {
  if (!input.is_object() || !input.contains("description") ||
      input.at("description").is_null()) return "Running task";
  return input.at("description");
}

std::string batGadgetProgressText(const Json& progressMessages){
  std::unordered_set<std::string> calls;
  std::string latest;
  if(progressMessages.is_array())for(const auto& event:progressMessages){
    const auto& data=value::property(event,"data");
    if(value::property(data,"type")!="agent_progress")continue;
    const auto& message=value::property(data,"message");
    if(value::property(message,"type")!="assistant")continue;
    const auto& blocks=value::property(value::property(message,"message"),"content");
    if(!blocks.is_array())continue;
    for(const auto& block:blocks){
      if(value::property(block,"type")!="tool_use")continue;
      const auto& id=value::property(block,"id");
      if(id.is_string())calls.insert(id.get<std::string>());
      const auto& name=value::property(block,"name");
      if(name.is_string())latest=name.get<std::string>();
    }
  }
  if(latest.empty())return "Working…";
  return "Latest tool: "+latest+" · "+std::to_string(calls.size())+
    (calls.size()==1?" tool call":" tool calls");
}

std::string groupedBatGadgetUseText(const Json& entries){
  if(!entries.is_array()||entries.empty())return {};
  std::string text=std::to_string(entries.size())+
    (entries.size()==1?" Bat":" Bats");
  for(const auto& entry:entries){
    const auto& input=value::property(value::property(entry,"param"),"input");
    const auto& description=value::property(input,"description");
    text+="\n  "+(description.is_string()?description.get<std::string>():"Agent task")+" · ";
    if(value::property(entry,"isError")==true)text+="Failed";
    else if(value::property(entry,"isResolved")==true)text+="Completed";
    else if(value::property(entry,"isInProgress")==true||
        (value::property(entry,"progressMessages").is_array()&&
         !value::property(entry,"progressMessages").empty()))
      text+=batGadgetProgressText(value::property(entry,"progressMessages"));
    else text+="Queued";
  }
  return text;
}
} // namespace batbox::bats
