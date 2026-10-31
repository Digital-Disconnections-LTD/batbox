#include "attachments/Aggregate.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace batbox;
namespace {
template<class T>T await(event_loop::Loop& loop,event_loop::Deferred<T> pending){
  std::optional<T> value;std::exception_ptr error;
  pending.then([&](const T* result,const std::exception_ptr& failure){
    error=failure;if(result)value=*result;
  });
  for(int i=0;i<2000&&!value&&!error;++i){
    loop.runReady();std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if(error)std::rethrow_exception(error);
  if(!value)throw std::runtime_error("Attachment operation did not settle");
  return *value;
}
Json run(const Json& input){
  event_loop::Loop loop;
  attachments::Services services{loop};
  Json trace=Json::array();
  auto echo=[&](attachments::Collector& collector,std::string label){
    collector=[&,label=std::move(label)](const attachments::Input&,
      execution::Context&){
      trace.push_back(Json::array({"start",label}));
      if(value::property(input,"fail")==label)
        throw std::runtime_error(label);
      event_loop::Deferred<Json> output(loop);
      if(value::property(input,"failAsync")==label)
        output.reject(std::make_exception_ptr(std::runtime_error(label)));
      else output.resolve(Json::array({Json{{"type",label}}}));
      return output;
    };
  };
  auto& c=services.collectors;
  echo(c.atMentionedFiles,"at_mentioned_files");
  echo(c.mcpResources,"mcp_resources");
  echo(c.batMentions,"agent_mentions");
  echo(c.skillDiscovery,"skill_discovery");
  echo(c.dateChange,"date_change");
  echo(c.ultrathinkEffort,"ultrathink_effort");
  echo(c.deferredGadgetsDelta,"deferred_tools_delta");
  echo(c.batListingDelta,"agent_listing_delta");
  echo(c.mcpInstructionsDelta,"mcp_instructions_delta");
  echo(c.companionIntro,"companion_intro");
  echo(c.changedFiles,"changed_files");
  echo(c.skillListing,"skill_listing");
  echo(c.planMode,"plan_mode");
  echo(c.planModeExit,"plan_mode_exit");
  echo(c.autoMode,"auto_mode");
  echo(c.autoModeExit,"auto_mode_exit");
  echo(c.todoReminders,"todo_reminders");
  echo(c.missionReminders,"todo_reminders");
  echo(c.partnerMailbox,"teammate_mailbox");
  echo(c.teamContext,"team_context");
  echo(c.batPendingMessages,"agent_pending_messages");
  echo(c.criticalSystemReminder,"critical_system_reminder");
  echo(c.compactionReminder,"compaction_reminder");
  echo(c.contextEfficiency,"context_efficiency");
  echo(c.selectedLinesIde,"ide_selection");
  echo(c.openedFileIde,"ide_opened_file");
  echo(c.outputStyle,"output_style");
  echo(c.diagnostics,"diagnostics");
  echo(c.lspDiagnostics,"lsp_diagnostics");
  echo(c.unifiedMissions,"unified_tasks");
  echo(c.asyncHookResponses,"async_hook_responses");
  echo(c.tokenUsage,"token_usage");
  echo(c.budgetUsd,"budget_usd");
  echo(c.outputTokenUsage,"output_token_usage");
  echo(c.verifyPlanReminder,"verify_plan_reminder");
  auto triggers=std::make_shared<attachment_triggers::Services>(loop);
  services.triggers=triggers;
  services.disableAttachments=[&]{return input.value("disabled",false);};
  services.simpleMode=[&]{return input.value("simple",false);};
  services.feature=[&](const std::string& key){
    trace.push_back(Json::array({"feature",key}));
    const auto& enabled=value::property(input,"features");
    if(enabled.is_array())for(const auto& value:enabled)if(value==key)return true;
    return false;
  };
  services.skillSearchModulesPresent=[&]{return input.value("skillModule",false);};
  services.isTodoV2Enabled=[&]{
    trace.push_back(Json::array({"todoV2"}));return input.value("todoV2",false);};
  services.isBatSwarmsEnabled=[&]{
    trace.push_back(Json::array({"swarms"}));return input.value("swarms",false);};
  services.logError=[&](const std::exception_ptr& error){
    try{std::rethrow_exception(error);}catch(const std::exception& failure){
      trace.push_back(Json::array({"error",failure.what()}));}
  };
  services.logDiagnostic=[&](const std::string& label,
    const std::exception_ptr& error){
    try{std::rethrow_exception(error);}catch(const std::exception& failure){
      trace.push_back(Json::array({"diagnostic",label,failure.what()}));}
  };
  services.randomUuid=[](){return std::string("u");};
  services.nowIso=[](){return std::string("t");};
  execution::Context context;
  context.data=Json::object();
  if(input.contains("agentId"))context.data["agentId"]=input["agentId"];
  context.nestedMemoryAttachmentTriggers=
    std::make_shared<runtime::OrderedStringSet>();
  context.dynamicSkillDirTriggers=
    std::make_shared<runtime::OrderedStringSet>();
  attachments::Input args;
  args.messages=Json::array();
  if(input.contains("text")&&!input["text"].is_null())
    args.text=input["text"].get<std::string>();
  args.querySource=input.value("querySource",std::string("repl"));
  args.skipSkillDiscovery=input.value("skipSkillDiscovery",false);
  if(input.value("mode",std::string())=="closed"){
    auto cursor=attachments::getAttachmentMessages(args,context,services);
    await(loop,cursor.close());
    if(await(loop,cursor.next()))throw std::logic_error("Closed cursor emitted a message");
    return {{"messages",Json::array()},{"trace",trace}};
  }
  if(input.value("mode",std::string())=="messages"){
    auto cursor=attachments::getAttachmentMessages(args,context,services);
    Json messages=Json::array();
    for(;;){
      auto item=await(loop,cursor.next());
      if(!item)break;
      messages.push_back(**item);
    }
    return {{"messages",messages},{"trace",trace}};
  }
  const auto result=await(loop,attachments::getAttachments(args,context,services));
  return {{"attachments",result},{"trace",trace}};
}
}
int main(){std::string line;while(std::getline(std::cin,line)){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify(Json{{"error",error.what()}})<<'\n';}
}}
