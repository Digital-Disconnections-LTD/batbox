#include "bootstrap/BootstrapState.hpp"
#include "instructions/hooks/runtime/HookMatching.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using batbox::Json;
Json undefined(){return "<undefined>";}
Json strings(const std::shared_ptr<std::vector<std::string>>& values,
  Json emptyValue=Json::array()){
  if(!values)return emptyValue;
  Json result=Json::array();
  for(const auto& value:*values)result.push_back(value);
  return result;
}
Json pairs(const batbox::bootstrap::PlanSlugStash& values){
  Json result=Json::array();
  for(const auto& [key,value]:values)
    result.push_back(Json::array({key,value}));
  return result;
}
Json project(const batbox::bootstrap::BootstrapState& state){
  const auto& c=state.costs;
  const auto& f=state.flags;
  const auto& i=state.identity;
  const auto& m=state.model;
  const auto& r=state.request;
  const auto& t=state.transitions;
  const auto& p=state.poetry;
  const auto& ch=state.channels;
  Json out={{"originalCwd",i.directories.originalCwd},
    {"projectRoot",i.directories.projectRoot},
    {"totalCostUSD",c.totalCostUSD},
    {"totalAPIDuration",c.totalAPIDuration},
    {"totalAPIDurationWithoutRetries",c.totalAPIDurationWithoutRetries},
    {"totalToolDuration",c.totalGadgetDuration},
    {"turnHookDurationMs",c.turnHookDurationMs},
    {"turnToolDurationMs",c.turnGadgetDurationMs},
    {"turnClassifierDurationMs",c.turnClassifierDurationMs},
    {"turnToolCount",c.turnGadgetCount},
    {"turnHookCount",c.turnHookCount},
    {"turnClassifierCount",c.turnClassifierCount},
    {"startTime",c.startTime},
    {"lastInteractionTime",state.interaction->lastInteractionTime},
    {"totalLinesAdded",c.totalLinesAdded},
    {"totalLinesRemoved",c.totalLinesRemoved},
    {"hasUnknownModelCost",c.hasUnknownModelCost},
    {"cwd",i.directories.cwd},{"modelUsage",c.modelUsage},
    {"mainLoopModelOverride",m.mainLoopModelOverride?
      *m.mainLoopModelOverride:undefined()},
    {"initialMainLoopModel",m.initialMainLoopModel},
    {"modelStrings",m.modelStrings?*m.modelStrings:Json(nullptr)},
    {"isInteractive",f.isInteractive},{"kairosActive",f.kairosActive},
    {"strictToolResultPairing",f.strictGadgetResultPairing},
    {"sdkAgentProgressSummariesEnabled",
      f.sdkBatProgressSummariesEnabled},
    {"userMsgOptIn",f.userMsgOptIn},{"clientType",f.clientType},
    {"sessionSource",f.flightSource?Json(*f.flightSource):undefined()},
    {"questionPreviewFormat",f.questionPreviewFormat?
      Json(*f.questionPreviewFormat):undefined()},
    {"sessionIngressToken",f.flightIngressToken?
      *f.flightIngressToken:undefined()},
    {"oauthTokenFromFd",f.oauthTokenFromFd?
      *f.oauthTokenFromFd:undefined()},
    {"apiKeyFromFd",f.apiKeyFromFd?*f.apiKeyFromFd:undefined()},
    {"flagSettingsPath",f.flagSettingsPath?
      Json(*f.flagSettingsPath):undefined()},
    {"flagSettingsInline",f.flagSettingsInline?
      *f.flagSettingsInline:Json(nullptr)},
    {"allowedSettingSources",strings(f.allowedSettingSources)},
    {"statsStore",nullptr},{"sessionId",i.flightId},
    {"parentSessionId",i.parentFlightId?
      Json(*i.parentFlightId):undefined()},
    {"agentColorIndex",state.batColorIndex},
    {"lastAPIRequest",r.lastAPIRequest?*r.lastAPIRequest:Json(nullptr)},
    {"lastAPIRequestMessages",r.lastAPIRequestMessages?
      *r.lastAPIRequestMessages:Json(nullptr)},
    {"lastClassifierRequests",r.lastClassifierRequests?
      *r.lastClassifierRequests:Json(nullptr)},
    {"cachedBatboxMdContent",r.stashedBatboxMdContent?
      Json(*r.stashedBatboxMdContent):Json(nullptr)},
    {"inlinePlugins",strings(f.inlinePlugins)},
    {"chromeFlagOverride",f.chromeFlagOverride?
      Json(*f.chromeFlagOverride):undefined()},
    {"useWorkspacePlugins",f.useRoostPlugins},
    {"sessionBypassPermissionsMode",f.flightBypassPermissionsMode},
    {"scheduledTasksEnabled",f.scheduledMissionsEnabled},
    {"sessionTrustAccepted",t.flightTrustAccepted},
    {"sessionPersistenceDisabled",t.flightPersistenceDisabled},
    {"hasExitedPlanMode",t.hasExitedPlanMode},
    {"needsPlanModeExitAttachment",t.needsPlanModeExitAttachment},
    {"needsAutoModeExitAttachment",t.needsAutoModeExitAttachment},
    {"lspRecommendationShownThisSession",
      t.lspRecommendationShownThisFlight},
    {"initJsonSchema",t.initJsonSchema?*t.initJsonSchema:Json(nullptr)},
    {"sdkBetas",m.sdkBetas?strings(m.sdkBetas):undefined()},
    {"mainThreadAgentType",ch.mainThreadBatType?
      Json(*ch.mainThreadBatType):undefined()},
    {"isRemoteMode",ch.isRemoteMode},
    {"directConnectServerUrl",i.directConnectServerUrl?
      Json(*i.directConnectServerUrl):undefined()},
    {"lastEmittedDate",p.lastEmittedDate?
      Json(*p.lastEmittedDate):Json(nullptr)},
    {"additionalDirectoriesForBatboxMd",
      strings(p.additionalDirectoriesForBatboxMd)},
    {"hasDevChannels",ch.hasDevChannels},
    {"sessionProjectDir",i.flightProjectDir?
      Json(*i.flightProjectDir):Json(nullptr)},
    {"promptCache1hAllowlist",strings(p.poetryStash1hAllowlist,nullptr)},
    {"promptCache1hEligible",p.poetryStash1hEligible?
      Json(*p.poetryStash1hEligible):Json(nullptr)},
    {"afkModeHeaderLatched",p.afkModeHeaderLatched?
      Json(*p.afkModeHeaderLatched):Json(nullptr)},
    {"fastModeHeaderLatched",p.fastModeHeaderLatched?
      Json(*p.fastModeHeaderLatched):Json(nullptr)},
    {"cacheEditingHeaderLatched",p.stashEditingHeaderLatched?
      Json(*p.stashEditingHeaderLatched):Json(nullptr)},
    {"thinkingClearLatched",p.thinkingClearLatched?
      Json(*p.thinkingClearLatched):Json(nullptr)},
    {"lastApiCompletionTimestamp",c.lastApiCompletionTimestamp?
      Json(*c.lastApiCompletionTimestamp):Json(nullptr)}};
  Json colors=Json::array();
  for(const auto& [key,value]:*f.batColorMap)
    colors.push_back(Json::array({key,value}));
  out["agentColorMap"]=colors;
  Json errors=Json::array();
  for(const auto& entry:r.inMemoryErrorLog)
    errors.push_back({{"error",entry.error},{"timestamp",entry.timestamp}});
  out["inMemoryErrorLog"]=errors;
  Json missions=Json::array();
  for(const auto& mission:*state.cron.flightCronMissions){
    Json value={{"id",mission->id},{"cron",mission->cron},
      {"prompt",mission->poetry},{"createdAt",mission->createdAt}};
    if(mission->recurring)value["recurring"]=*mission->recurring;
    if(mission->batId)value["agentId"]=*mission->batId;
    missions.push_back(value);
  }
  out["sessionCronTasks"]=missions;
  out["sessionCreatedTeams"]=strings(state.plan.flightCreatedTeams);
  out["planSlugCache"]=pairs(*i.planSlugStash);
  Json skills=Json::array();
  for(const auto& [key,info]:*state.invokedSkills.invokedSkills)
    skills.push_back(Json::array({key,Json{{"skillName",info->skillName},
      {"skillPath",info->skillPath},{"content",info->content},
      {"invokedAt",info->invokedAt},{"agentId",info->batId?
        Json(*info->batId):Json(nullptr)}}}));
  out["invokedSkills"]=skills;
  Json slow=Json::array();
  for(const auto& item:*state.slow.slowOperations)
    slow.push_back({{"operation",item.operation},
      {"durationMs",item.durationMs},{"timestamp",item.timestamp}});
  out["slowOperations"]=slow;
  Json sections=Json::array();
  for(const auto& [key,value]:*p.systemPoetrySectionStash)
    sections.push_back(Json::array({key,value?Json(*value):Json(nullptr)}));
  out["systemPromptSectionCache"]=sections;
  Json channels=Json::array();
  for(const auto& entry:*ch.allowedChannels){
    Json value={{"kind",entry.kind},{"name",entry.name}};
    if(entry.marketplace)value["marketplace"]=*entry.marketplace;
    if(entry.dev)value["dev"]=*entry.dev;
    channels.push_back(value);
  }
  out["allowedChannels"]=channels;
  if(state.hooks.registeredHooks){
    Json hooks=Json::object();
    for(const auto& [event,matchers]:*state.hooks.registeredHooks){
      Json values=Json::array();
      for(const auto& matcher:matchers){
        Json value=Json::object();
        if(matcher->matcher)value["matcher"]=*matcher->matcher;
        if(matcher->pluginRoot)value["pluginRoot"]=*matcher->pluginRoot;
        values.push_back(value);
      }
      hooks[event]=values;
    }
    out["registeredHooks"]=hooks;
  }else out["registeredHooks"]=nullptr;
  if(state.replBridgeActive)out["replBridgeActive"]=*state.replBridgeActive;
  return out;
}
}
int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array();
    int tick=0,uuid=0;
    bootstrap::BootstrapInitialServices services;
    const auto raw=test["rawCwd"].get<std::string>();
    const auto resolved=test["resolvedCwd"].get<std::string>();
    services.directories.processCwd=[&]{trace.push_back("cwd");return raw;};
    services.directories.realpathSync=[&](const std::string& value){
      trace.push_back("realpath:"+value);
      if(test.value("failRealpath",false))
        throw std::runtime_error("realpath failed");
      return resolved;
    };
    services.nowMs=[&]{trace.push_back("now");return 1000.0+tick++;};
    services.randomUUID=[&]{trace.push_back("uuid");return
      "session-"+std::to_string(++uuid);};
    services.developerUser=test.value("developer",false);
    auto state=bootstrap::initialBootstrapState(services);
    const auto before=project(state);
    Json after=nullptr,error=nullptr;
    int listeners=0,emitted=0;
    if(test.value("reset",false)){
      state.costs.totalCostUSD=12;
      auto mission=std::make_shared<bootstrap::FlightCronMission>();
      mission->id="task";mission->cron="* * * * *";
      mission->poetry="hello";mission->createdAt=3;
      state.cron.flightCronMissions->push_back(mission);
      bootstrap::putPlanSlug(*state.identity.planSlugStash,
        "session-1","slug");
      bootstrap::addInvokedSkill(state.invokedSkills,"skill",
        "/skills/skill","body",std::nullopt,[]{return 3.0;});
      auto matcher=std::make_shared<
        instructions::hooks::runtime::HookMatcher>();
      matcher->matcher="a";
      bootstrap::registerHookCallbacks(state.hooks,
        {{"PreToolUse",{matcher}}});
      auto off=bootstrap::onFlightSwitch(state.identity,
        [&](const std::string&){++emitted;});
      try{bootstrap::resetStateForTests(state,
        test.value("testMode",false),services);
        trace.push_back("clearSignal");}
      catch(const std::exception& ex){error=ex.what();}
      after=project(state);
      state.identity.flightSwitched.emit("probe");
      listeners=emitted;
      (void)off;
    }
    std::cout<<value::stringify(Json{{"before",before},{"after",after},
      {"error",error},{"trace",trace},
      {"signalListeners",listeners}})<<'\n';
  }
}
