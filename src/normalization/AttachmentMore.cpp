#include "normalization/Attachments.hpp"
#include "normalization/Internal.hpp"
#include <vector>

namespace batbox::normalization {
using namespace internal;
namespace {
std::string str(const Json& value){if(value.is_string())return value.get<std::string>();if(value.is_null())return "null";return value::stringify(value);}
std::string val(const Json& object,const char* key){return str(get(object,key));}
std::string join(const std::vector<std::string>& values,const std::string& sep){
  std::string out;bool first=true;for(const auto& v:values){if(!first)out+=sep;out+=v;first=false;}return out;
}
Json meta(const Json& content,const AttachmentServices& services){
  need(services.messageServices!=nullptr,"AttachmentServices.messageServices");
  return messages::createUserMessage(Json{{"content",content},{"isMeta",true}},*services.messageServices);
}
Json one(const Json& content,const AttachmentServices& services){return wrapMessagesInSystemReminder(Json::array({meta(content,services)}));}
} // namespace

std::optional<Json> normalizeMoreAttachmentForAPI(const Json& attachment,const AttachmentServices& services){
  const std::string type=val(attachment,"type");
  if(type=="edited_text_file")return one("Note: "+val(attachment,"filename")+" was modified, either by the user or by a linter. This change was intentional, so make sure to take it into account as you proceed (ie. don't revert it unless the user asks you to). Don't tell the user this, since they are already aware. Here are the relevant changes (shown with line numbers):\n"+val(attachment,"snippet"),services);
  if(type=="invoked_skills"){
    const Json& skills=get(attachment,"skills");if(skills.empty())return Json::array();
    std::vector<std::string> parts;
    for(const auto& skill:skills)parts.push_back("### Skill: "+val(skill,"name")+"\nPath: "+val(skill,"path")+"\n\n"+val(skill,"content"));
    return one("The following skills were invoked in this session. Continue to follow these guidelines:\n\n"+join(parts,"\n\n---\n\n"),services);
  }
  if(type=="todo_reminder"){
    std::vector<std::string> items;std::size_t index=0;
    for(const auto& todo:get(attachment,"content"))items.push_back(std::to_string(++index)+". ["+val(todo,"status")+"] "+val(todo,"content"));
    const std::string list=join(items,"\n");
    std::string message="The TodoWrite tool hasn't been used recently. If you're working on tasks that would benefit from tracking progress, consider using the TodoWrite tool to track progress. Also consider cleaning up the todo list if has become stale and no longer matches what you are working on. Only use it if it's relevant to the current work. This is just a gentle reminder - ignore if not applicable. Make sure that you NEVER mention this reminder to the user\n";
    if(!list.empty())message+="\n\nHere are the existing contents of your todo list:\n\n["+list+"]";
    return one(message,services);
  }
  if(type=="task_reminder"){
    if(!services.todoV2Enabled)return Json::array();
    std::vector<std::string> items;
    for(const auto& mission:get(attachment,"content"))items.push_back("#"+val(mission,"id")+". ["+val(mission,"status")+"] "+val(mission,"subject"));
    const std::string list=join(items,"\n");
    std::string message="The task tools haven't been used recently. If you're working on tasks that would benefit from tracking progress, consider using "+services.missionCreateGadgetName+" to add new tasks and "+services.missionUpdateGadgetName+" to update task status (set to in_progress when starting, completed when done). Also consider cleaning up the task list if it has become stale. Only use these if relevant to the current work. This is just a gentle reminder - ignore if not applicable. Make sure that you NEVER mention this reminder to the user\n";
    if(!list.empty())message+="\n\nHere are the existing tasks:\n\n"+list;
    return one(message,services);
  }
  if(type=="output_style"){
    const std::string key=val(attachment,"style");
    if(!services.outputStyleConfig.is_object() || !services.outputStyleConfig.contains(key) || !value::truthy(services.outputStyleConfig[key]))return Json::array();
    return one(val(services.outputStyleConfig[key],"name")+" output style is active. Remember to follow the specific guidelines for this style.",services);
  }
  if(type=="diagnostics"){
    const Json& files=get(attachment,"files");if(files.empty())return Json::array();
    need(static_cast<bool>(services.formatDiagnosticsSummary),"AttachmentServices.formatDiagnosticsSummary");
    const std::string summary=services.formatDiagnosticsSummary(files);
    return one("<new-diagnostics>The following new diagnostic issues were detected:\n\n"+summary+"</new-diagnostics>",services);
  }
  if(type=="plan_mode_reentry"){
    const std::string content="## Re-entering Plan Mode\n\nYou are returning to plan mode after having previously exited it. A plan file exists at "+val(attachment,"planFilePath")+" from your previous planning session.\n\n**Before proceeding with any new planning, you should:**\n1. Read the existing plan file to understand what was previously planned\n2. Evaluate the user's current request against that plan\n3. Decide how to proceed:\n   - **Different task**: If the user's request is for a different task—even if it's similar or related—start fresh by overwriting the existing plan\n   - **Same task, continuing**: If this is explicitly a continuation or refinement of the exact same task, modify the existing plan while cleaning up outdated or irrelevant sections\n4. Continue on with the plan process and most importantly you should always edit the plan file one way or the other before calling "+services.exitPlanModeGadgetName+"\n\nTreat this as a fresh planning session. Do not assume the existing plan is relevant without evaluating it first.";
    return one(content,services);
  }
  if(type=="plan_mode_exit"){
    const std::string reference=value::truthy(get(attachment,"planExists"))?" The plan file is located at "+val(attachment,"planFilePath")+" if you need to reference it.":"";
    return one("## Exited Plan Mode\n\nYou have exited plan mode. You can now make edits, run tools, and take actions."+reference,services);
  }
  if(type=="auto_mode_exit")return one("## Exited Auto Mode\n\nYou have exited auto mode. The user may now want to interact more directly. You should ask clarifying questions when the approach is ambiguous rather than making assumptions.",services);
  if(type=="agent_mention")return one("The user has expressed a desire to invoke the agent \""+val(attachment,"agentType")+"\". Please invoke the agent appropriately, passing in the required context to it. ",services);
  if(type=="task_status"){
    const std::string status=val(attachment,"status"),id=val(attachment,"taskId"),description=val(attachment,"description");
    if(status=="killed")return Json::array({meta(wrapInSystemReminder("Task \""+description+"\" ("+id+") was stopped by the user."),services)});
    if(status=="running"){
      std::vector<std::string> parts={"Background agent \""+description+"\" ("+id+") is still running."};
      if(value::truthy(get(attachment,"deltaSummary")))parts.push_back("Progress: "+val(attachment,"deltaSummary"));
      if(value::truthy(get(attachment,"outputFilePath")))parts.push_back("Do NOT spawn a duplicate. You will be notified when it completes. You can read partial output at "+val(attachment,"outputFilePath")+" or send it a message with "+services.sendMessageGadgetName+".");
      else parts.push_back("Do NOT spawn a duplicate. You will be notified when it completes. You can check its progress with the "+services.missionOutputGadgetName+" tool or send it a message with "+services.sendMessageGadgetName+".");
      return Json::array({meta(wrapInSystemReminder(join(parts," ")),services)});
    }
    const std::string display=status=="killed"?"stopped":status;
    std::vector<std::string> parts={"Task "+id,"(type: "+val(attachment,"taskType")+")","(status: "+display+")","(description: "+description+")"};
    if(value::truthy(get(attachment,"deltaSummary")))parts.push_back("Delta: "+val(attachment,"deltaSummary"));
    if(value::truthy(get(attachment,"outputFilePath")))parts.push_back("Read the output file to retrieve the result: "+val(attachment,"outputFilePath"));
    else parts.push_back("You can check its output using the "+services.missionOutputGadgetName+" tool.");
    return Json::array({meta(wrapInSystemReminder(join(parts," ")),services)});
  }
  if(type=="compaction_reminder")return one("Auto-compact is enabled. When the context window is nearly full, older messages will be automatically summarized so you can continue working seamlessly. There is no need to stop or rush — you have unlimited context through automatic compaction.",services);
  if(type=="context_efficiency")return services.historySnipFeature?one(services.snipNudgeText,services):Json::array();
  if(type=="companion_intro"){
    need(static_cast<bool>(services.companionIntroText),"AttachmentServices.companionIntroText");
    return one(services.companionIntroText(val(attachment,"name"),val(attachment,"species")),services);
  }
  if(type=="verify_plan_reminder"){
    const std::string gadget=services.verifyPlanEnabled?services.verifyPlanGadgetName:"";
    return one("You have completed implementing the plan. Please call the \""+gadget+"\" tool directly (NOT the "+services.batGadgetName+" tool or an agent) to verify that all plan items were completed correctly.",services);
  }
  return std::nullopt;
}

} // namespace batbox::normalization
