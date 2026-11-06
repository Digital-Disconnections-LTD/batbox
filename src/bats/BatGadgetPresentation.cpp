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

Json mapBatGadgetResult(const Json& data,const std::string& gadgetUseId) {
  if (hasStatus(data,"async_launched")) {
    const auto id=interpolated(data,"agentId");
    const std::string prefix="Async agent launched successfully.\nagentId: "+id+
      " (internal ID - do not mention to user. Use SendMessage with to: '"+id+
      "' to continue this agent.)\nThe agent is working in the background. Its result "
      "will come back to you as a notification when it finishes. Do not launch another "
      "agent for the same work.";
    // Older results carry no canWaitForResult; TaskOutput is a core Gadget.
    const bool canWait=!data.contains("canWaitForResult") ||
      value::truthy(data.at("canWaitForResult"));
    const std::string instructions=canWait?
      "- If your next step needs this agent's result, wait for it: call TaskOutput with "
      "task_id: \""+id+"\" and block: true. It returns the result when the agent finishes.\n"
      "- If you have other work that does not depend on the result, do that first, keeping "
      "away from the files and topics the agent is working on. Then wait with TaskOutput "
      "if you still need the result.":
      "Briefly tell the user what you launched and end your response. Do not generate any other text — agent results will arrive in a subsequent message.";
    return resultBlock(gadgetUseId,Json::array({contentText(prefix+"\n"+instructions)}));
  }
  if (hasStatus(data,"queued_for_slot")) {
    return resultBlock(gadgetUseId,Json::array({contentText(
      "No Bat slot was free, and the user sent a new message, so this launch returned "
      "without waiting. The agent is queued: it starts in the background as soon as a "
      "slot frees, and its result will come back to you as a notification when it "
      "finishes. It has no agentId until it starts. Do not launch another agent for "
      "the same work; read the user's message first."+
      (value::truthy(value::property(data,"taskId"))?
        " To cancel it before it starts, call TaskStop with task_id: \""+
        interpolated(data,"taskId")+"\".":std::string()))}));
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

std::string batLaunchedBy(const Json& progressMessages){
  if(progressMessages.is_array())for(const auto& event:progressMessages){
    const auto& by=value::property(value::property(event,"data"),"launchedBy");
    if(by.is_string()&&!by.get_ref<const std::string&>().empty())return by.get<std::string>();
  }
  return {};
}
namespace {
std::string latestToolText(const Json& progressMessages){
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
  if(latest.empty())return {};
  return "Latest tool: "+latest+" · "+std::to_string(calls.size())+
    (calls.size()==1?" tool call":" tool calls");
}
std::string joined(const std::string& first,const std::string& second){
  if(first.empty())return second;
  return second.empty()?first:first+" · "+second;
}
}
std::string batGadgetProgressText(const Json& progressMessages){
  const auto latest=latestToolText(progressMessages);
  return latest.empty()?std::string("Working…"):latest;
}

std::string groupedBatGadgetUseText(const Json& entries){
  if(!entries.is_array()||entries.empty())return {};
  std::string text=std::to_string(entries.size())+
    (entries.size()==1?" Bat":" Bats");
  for(const auto& entry:entries){
    const auto& input=value::property(value::property(entry,"param"),"input");
    const auto& description=value::property(input,"description");
    text+="\n  "+(description.is_string()?description.get<std::string>():"Agent task")+" · ";
    // A sub-Bat names the Bat that launched it ahead of its status.
    const auto& progress=value::property(entry,"progressMessages");
    const auto by=batLaunchedBy(progress);
    std::string state;
    // A background launch resolves at once; its result is the launch, not
    // the Bat's outcome, which arrives later as a notification.
    const auto& status=value::property(value::property(entry,"output"),"status");
    if(value::property(entry,"isError")==true)state="Failed";
    else if(status=="async_launched")
      state=by.empty()?"Launched":"";
    else if(status=="queued_for_slot")state="Waiting for a slot";
    else if(value::property(entry,"isResolved")==true)state="Completed";
    else if(value::property(entry,"isInProgress")==true||
        (progress.is_array()&&!progress.empty()))
      state=by.empty()?batGadgetProgressText(progress):latestToolText(progress);
    else state=by.empty()?"Queued":"";
    text+=joined(by.empty()?"":"Launched by "+by,state);
  }
  return text;
}
} // namespace batbox::bats
