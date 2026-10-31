#include "attachments/context/ContextCollectors.hpp"
#include <stdexcept>

using namespace batbox;
namespace {
void check(bool valid,const char* message){
  if(!valid)throw std::runtime_error(message);
}
Json await(event_loop::Loop& loop,event_loop::Deferred<Json> pending){
  std::optional<Json> value;
  std::exception_ptr error;
  pending.then([&](const Json* result,const std::exception_ptr& failure){
    if(result)value=*result;
    error=failure;
  });
  for(int i=0;i<100&&!value&&!error;++i)loop.runReady();
  if(error)std::rethrow_exception(error);
  if(!value)throw std::runtime_error("Aggregate did not settle");
  return *value;
}
bool contains(const Json& attachments,const char* type){
  for(const auto& item:attachments)if(value::property(item,"type")==type)return true;
  return false;
}
const Json& find(const Json& attachments,const char* type){
  for(const auto& item:attachments)if(value::property(item,"type")==type)return item;
  throw std::runtime_error(std::string("Missing attachment: ")+type);
}
void emptyCollectors(attachments::Services& services){
  auto empty=[&](const attachments::Input&,execution::Context&){
    event_loop::Deferred<Json> result(services.loop);
    result.resolve(Json::array());return result;
  };
  auto& c=services.collectors;
  c.atMentionedFiles=empty;c.mcpResources=empty;c.batMentions=empty;
  c.skillDiscovery=empty;c.companionIntro=empty;c.changedFiles=empty;
  c.skillListing=empty;c.planMode=empty;c.planModeExit=empty;
  c.autoMode=empty;c.autoModeExit=empty;c.todoReminders=empty;
  c.missionReminders=empty;c.partnerMailbox=empty;c.teamContext=empty;
  c.batPendingMessages=empty;c.compactionReminder=empty;
  c.contextEfficiency=empty;c.selectedLinesIde=empty;c.openedFileIde=empty;
  c.diagnostics=empty;c.lspDiagnostics=empty;c.unifiedMissions=empty;
  c.asyncHookResponses=empty;c.verifyPlanReminder=empty;
}
}
int main(){
  event_loop::Loop loop;
  attachments::Services aggregate{loop};
  emptyCollectors(aggregate);
  aggregate.triggers=std::make_shared<attachment_triggers::Services>(loop);
  aggregate.disableAttachments=[](){return false;};
  aggregate.simpleMode=[](){return false;};
  aggregate.feature=[](const std::string&){return false;};
  aggregate.skillSearchModulesPresent=[](){return false;};
  aggregate.isTodoV2Enabled=[](){return false;};
  aggregate.isBatSwarmsEnabled=[](){return false;};
  std::vector<std::string> errors;
  aggregate.logError=[&](std::exception_ptr error){
    try{std::rethrow_exception(error);}catch(const std::exception& failure){
      errors.push_back(failure.what());
    }
  };
  aggregate.logDiagnostic=[](const std::string&,std::exception_ptr){};
  aggregate.randomUuid=[](){return std::string("id");};
  aggregate.nowIso=[](){return std::string("2026-10-29T00:00:00Z");};
  bootstrap::PoetryState poetry;
  bootstrap::CostState cost;
  cost.totalCostUSD=2;
  cost.modelUsage={{"standard",{{"outputTokens",30}}}};
  cost.outputTokensAtTurnStart=10;
  cost.currentTurnTokenBudget=100;
  std::string date="2026-10-29";
  auto app=std::make_shared<app_state::AppState>();
  app->permissionContext=std::make_shared<value::SharedRecord>(
    value::SharedRecord::fromJson(Json{{"blocked",Json::array()}}));
  attachments::context::Services contextServices;
  contextServices.leaves.poetryState=&poetry;
  contextServices.leaves.costState=&cost;
  contextServices.leaves.localIsoDate=[&]{return date;};
  contextServices.leaves.feature=[](const std::string& key){
    return key=="TOKEN_BUDGET";
  };
  contextServices.leaves.ultrathinkEnabled=[](){return true;};
  contextServices.leaves.getSettings=[](){return Json{{"outputStyle","concise"}};};
  contextServices.deferredGadgetsDeltaEnabled=[](){return true;};
  contextServices.gadgetSearchEnabledOptimistic=[](){return true;};
  contextServices.batListInMessages=[](){return true;};
  contextServices.mcpInstructionsDeltaEnabled=[](){return true;};
  contextServices.tokenUsageAttachmentEnabled=[](){return true;};
  contextServices.modelSupportsGadgetReference=[](const std::string&){return true;};
  contextServices.clientSideInstruction=[](){
    return std::optional<attachments::context::ClientInstruction>{};
  };
  contextServices.deferral.feature=[](const std::string&){return false;};
  contextServices.getAppState=[&]{return app;};
  contextServices.filterDeniedBats=[](const Json& bats,const Json&,
      const std::string&){return bats;};
  contextServices.subscriptionType=[](){return std::string("free");};
  contextServices.effectiveContextWindow=[](const std::string&){return 1000.0;};
  attachments::context::bindContextCollectors(aggregate,contextServices);
  gadgets::Gadget search;search.name="ToolSearch";
  gadgets::Gadget bat;bat.name="Agent";
  gadgets::Gadget deferred;deferred.name="ReadLater";deferred.shouldDefer=true;
  execution::Context context;
  context.gadgets={&search,&bat,&deferred};
  context.nestedMemoryAttachmentTriggers=
    std::make_shared<runtime::OrderedStringSet>();
  context.dynamicSkillDirTriggers=
    std::make_shared<runtime::OrderedStringSet>();
  context.data={{"options",{{"mainLoopModel","standard"},
    {"maxBudgetUsd",10},{"mcpClients",Json::array({Json{
      {"type","connected"},{"name","docs"},{"instructions","Read docs"}}})},
    {"agentDefinitions",{{"activeAgents",Json::array({Json{
      {"agentType","research"},{"whenToUse","Inspect docs"}}})},
      {"allowedAgentTypes",nullptr}}}}}};
  attachments::Input firstInput;
  firstInput.text="ultrathink";
  firstInput.messages=Json::array();
  const auto first=await(loop,attachments::getAttachments(firstInput,context,
    aggregate));
  check(errors.empty(),"First aggregate logged a collector error");
  check(!contains(first,"date_change"),"Initial date should latch without emission");
  check(contains(first,"ultrathink_effort"),"Effort attachment missing");
  check(find(first,"deferred_tools_delta").at("addedNames")==
    Json::array({"ReadLater"}),"Deferred tool announcement missing");
  check(find(first,"agent_listing_delta").at("addedTypes")==
    Json::array({"research"}),"Agent announcement missing");
  check(find(first,"mcp_instructions_delta").at("addedNames")==
    Json::array({"docs"}),"MCP instruction announcement missing");
  check(contains(first,"output_style")&&contains(first,"token_usage")&&
    contains(first,"budget_usd")&&contains(first,"output_token_usage"),
    "Main-thread style or budget attachment missing");
  Json previous=Json::array();
  for(const auto& item:first)previous.push_back(
    Json{{"type","attachment"},{"attachment",item}});
  previous.push_back(Json{{"type","assistant"},{"message",{
    {"model","standard"},{"content",Json::array({Json{
      {"type","text"},{"text","Done"}}})},
    {"usage",{{"input_tokens",100},{"output_tokens",20}}}}}});
  date="2026-10-30";
  context.gadgets={&search,&bat};
  context.data["options"]["mcpClients"]=Json::array();
  context.data["options"]["agentDefinitions"]["activeAgents"]=Json::array();
  attachments::Input secondInput;
  secondInput.messages=std::move(previous);
  const auto second=await(loop,attachments::getAttachments(secondInput,context,
    aggregate));
  check(errors.empty(),"Second aggregate logged a collector error");
  check(find(second,"date_change").at("newDate")==date,
    "Date state did not advance");
  check(find(second,"deferred_tools_delta").at("removedNames")==
    Json::array({"ReadLater"}),"Deferred tool removal missing");
  check(find(second,"agent_listing_delta").at("removedTypes")==
    Json::array({"research"}),"Agent removal missing");
  check(find(second,"mcp_instructions_delta").at("removedNames")==
    Json::array({"docs"}),"MCP instruction removal missing");
  check(find(second,"token_usage").at("used")==120,
    "Token usage did not use the latest model response");
  check(poetry.lastEmittedDate=="2026-10-30","Date latch was lost");
}
