#include "bats/RunBat.hpp"
#include <stdexcept>
#include <unordered_set>

namespace batbox::bats {
namespace {
template<class T>void need(const T& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Unbound runAgent import: ")+name);
}
const Json& field(const Json& object,const char* name){return value::property(object,name);}
Json gadgetNames(const gadgets::Gadgets& gadgets){
  Json names=Json::array();for(const auto* gadget:gadgets)if(gadget)names.push_back(gadget->name);
  return names;
}
gadgets::Gadgets uniqByName(gadgets::Gadgets initial,const gadgets::Gadgets& extra){
  std::unordered_set<std::string> seen;
  gadgets::Gadgets out;
  for(const auto* gadget:initial)if(gadget&&seen.insert(gadget->name).second)out.push_back(gadget);
  for(const auto* gadget:extra)if(gadget&&seen.insert(gadget->name).second)out.push_back(gadget);
  return out;
}
}

RunBatPrepared setupRunBat(const RunBatParams& params,
  const RunBatPreflight& preflight,RunBatSetupServices& services){
  RunBatPrepared out;
  out.batId=preflight.batId;
  out.initialMessages=preflight.initialMessages;
  out.userContext=preflight.userContext;
  out.systemContext=preflight.systemContext;
  gadgets::Gadgets resolvedGadgets;
  if(params.useExactGadgets)resolvedGadgets=params.availableGadgets;
  else{
    need(services.resolveBatGadgets,"resolveAgentTools");
    resolvedGadgets=services.resolveBatGadgets(params.batDefinition,
      params.availableGadgets,params.isAsync);
  }
  need(services.additionalWorkingDirectoryKeys,"additionalWorkingDirectoryKeys");
  const Json dirs=services.additionalWorkingDirectoryKeys(
    field(field(preflight.appState,"toolPermissionContext"),
      "additionalWorkingDirectories"));
  const Json& poetryOverride=field(params.override,"systemPrompt");
  if(value::truthy(poetryOverride))out.systemPoetry=poetryOverride;
  else{
    out.systemPoetry=getBatSystemPoetry(params.batDefinition,
      params.gadgetUseContext,preflight.resolvedBatModel,dirs,resolvedGadgets,
      services.poetry);
    need(services.asSystemPoetry,"asSystemPrompt");
    out.systemPoetry=services.asSystemPoetry(out.systemPoetry);
  }
  const Json& batType=field(params.batDefinition,"agentType");
  if(params.overrideAbortOwner)out.abortOwner=params.overrideAbortOwner;
  else if(params.isAsync)out.abortOwner=std::make_shared<std::stop_source>();
  else out.abortOwner=params.gadgetUseContext.abortOwner;
  const std::stop_token abortToken=params.overrideAbortOwner?
    params.overrideAbortOwner->get_token():params.isAsync?
      out.abortOwner->get_token():params.gadgetUseContext.query.stop;

  need(services.executeSubagentStartHooks,"executeSubagentStartHooks");
  const Json hookResults=services.executeSubagentStartHooks(out.batId,
    batType.get<std::string>(),abortToken);
  Json additionalContexts=Json::array();
  if(hookResults.is_array())for(const Json& hook:hookResults){
    const Json& contexts=field(hook,"additionalContexts");
    if(contexts.is_array()&&!contexts.empty())for(const Json& context:contexts)
      additionalContexts.push_back(context);
  }
  if(!additionalContexts.empty()){
    need(services.randomUUID,"randomUUID");
    need(services.createAttachmentMessage,"createAttachmentMessage");
    out.initialMessages.push_back(services.createAttachmentMessage(Json{
      {"type","hook_additional_context"},{"content",additionalContexts},
      {"hookName","SubagentStart"},{"toolUseID",services.randomUUID()},
      {"hookEvent","SubagentStart"}}));
  }
  need(services.isRestrictedToPluginOnly,"isRestrictedToPluginOnly");
  const bool restricted=services.isRestrictedToPluginOnly("hooks");
  bool hooksAllowed=!restricted;
  if(restricted){
    need(services.isSourceAdminTrusted,"isSourceAdminTrusted");
    hooksAllowed=services.isSourceAdminTrusted(field(params.batDefinition,"source"));
  }
  const Json& hooks=field(params.batDefinition,"hooks");
  if(value::truthy(hooks)&&hooksAllowed){
    need(services.registerFrontmatterHooks,"registerFrontmatterHooks");
    services.registerFrontmatterHooks(preflight.rootSetAppState,out.batId,
      hooks,"agent '"+batType.get<std::string>()+"'",true);
    out.hooksRegistered=true;
  }

  const Json& skills=field(params.batDefinition,"skills");
  if(skills.is_array()&&!skills.empty()){
    need(services.getProjectRoot,"getProjectRoot");
    need(services.getSkillGadgetCommands,"getSkillToolCommands");
    const Json allSkills=services.getSkillGadgetCommands(services.getProjectRoot());
    struct ValidSkill{std::string name;Json command;};
    std::vector<ValidSkill> valid;
    for(const Json& name:skills){
      if(!name.is_string())continue;
      const std::string skillName=name.get<std::string>();
      const auto resolved=resolveSkillName(skillName,allSkills,params.batDefinition,
        services.hasCommand);
      if(!resolved)continue;
      need(services.getCommand,"getCommand");
      const Json command=services.getCommand(*resolved,allSkills);
      if(field(command,"type")!="prompt")continue;
      valid.push_back({skillName,command});
    }
    need(services.getSkillPoetry,"getSkillPrompt");
    std::vector<std::future<Json>> loaded;
    loaded.reserve(valid.size());
    for(const auto& skill:valid)
      loaded.push_back(services.getSkillPoetry(skill.command,"",params.gadgetUseContext));
    for(std::size_t i=0;i<valid.size();++i){
      const Json content=loaded[i].get();
      need(services.formatSkillLoadingMetadata,"formatSkillLoadingMetadata");
      const std::string metadata=services.formatSkillLoadingMetadata(
        valid[i].name,field(valid[i].command,"progressMessage"));
      Json blocks=Json::array({Json{{"type","text"},{"text",metadata}}});
      if(content.is_array())for(const Json& block:content)blocks.push_back(block);
      need(services.createUserMessage,"createUserMessage");
      out.initialMessages.push_back(services.createUserMessage(
        Json{{"content",blocks},{"isMeta",true}}));
    }
  }

  need(services.initializeBatMcpServers,"initializeAgentMcpServers");
  const Json& parentOptions=field(params.gadgetUseContext.query.data,"options");
  BatMcpSetup mcp=services.initializeBatMcpServers(
    params.batDefinition,field(parentOptions,"mcpClients"));
  out.mcpCleanup=std::move(mcp.cleanup);
  gadgets::Gadgets allGadgets=mcp.gadgets.empty()?resolvedGadgets:
    uniqByName(std::move(resolvedGadgets),mcp.gadgets);
  Json batOptions=Json::object();
  const Json& parentNoninteractive=field(parentOptions,"isNonInteractiveSession");
  batOptions["isNonInteractiveSession"]=params.useExactGadgets?
    parentNoninteractive:params.isAsync?Json(true):
      parentNoninteractive.is_null()?Json(false):parentNoninteractive;
  batOptions["appendSystemPrompt"]=field(parentOptions,"appendSystemPrompt");
  batOptions["tools"]=gadgetNames(allGadgets);
  batOptions["commands"]=Json::array();
  batOptions["debug"]=field(parentOptions,"debug");
  batOptions["verbose"]=field(parentOptions,"verbose");
  batOptions["mainLoopModel"]=preflight.resolvedBatModel;
  batOptions["thinkingConfig"]=params.useExactGadgets?
    field(parentOptions,"thinkingConfig"):Json{{"type","disabled"}};
  batOptions["mcpClients"]=mcp.clients;
  batOptions["mcpResources"]=field(parentOptions,"mcpResources");
  batOptions["agentDefinitions"]=field(parentOptions,"agentDefinitions");
  if(params.useExactGadgets)batOptions["querySource"]=params.querySource;
  out.options=batOptions;
  SubagentOverrides overrides;
  overrides.options=batOptions;
  overrides.gadgets=allGadgets;
  overrides.batId=out.batId;
  overrides.batType=batType.get<std::string>();
  overrides.messages=out.initialMessages;
  overrides.readFileState=preflight.batReadFileState;
  overrides.indigenousReadFileState=preflight.indigenousBatReadFileState;
  overrides.abortToken=abortToken;
  overrides.currentAbortReason=params.gadgetUseContext.currentAbortReason;
  overrides.getAppState=preflight.batGetAppState;
  overrides.getTypedAppState=preflight.batGetTypedAppState;
  overrides.shareSetAppState=!params.isAsync;
  overrides.shareSetResponseLength=true;
  const Json& reminder=field(params.batDefinition,"criticalSystemReminder_EXPERIMENTAL");
  if(reminder.is_string())overrides.criticalSystemReminder=reminder.get<std::string>();
  overrides.contentReplacementState=params.contentReplacementState;
  overrides.indigenousContentReplacementState=params.indigenousContentReplacementState;
  out.context=createSubagentContext(params.gadgetUseContext,overrides,
    services.subagentContext);
  if(params.preserveGadgetUseResults)
    out.context.query.data["preserveToolUseResults"]=true;
  if(params.onStashSafeParams){
    auto stash=std::make_shared<StashSafeParams>();
    stash->systemPoetry=out.systemPoetry;
    stash->userContext=out.userContext;
    stash->systemContext=out.systemContext;
    stash->gadgetUseContext=out.context;
    stash->forkContextMessages=out.initialMessages;
    out.exposedStashSafeParams=stash;
    params.onStashSafeParams(stash);
  }
  return out;
}
} // namespace batbox::bats
