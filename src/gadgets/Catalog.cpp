#include "gadgets/Catalog.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <unordered_set>

namespace batbox::gadgets {
namespace {
const Gadget* requireGadget(const Gadget* gadget, const char* importName) {
  if (!gadget) throw std::invalid_argument(std::string("Unbound catalog tool import: ") + importName);
  return gadget;
}
void add(Gadgets& out, const Gadget* gadget, const char* importName) { out.push_back(requireGadget(gadget, importName)); }
void addIf(Gadgets& out, bool condition, const Gadget* gadget, const char* importName) {
  if (condition) add(out, gadget, importName);
}
Gadgets enabledGadgets(const Gadgets& gadgets) {
  std::vector<bool> enabled;
  enabled.reserve(gadgets.size());
  for (const Gadget* gadget : gadgets) {
    if (!gadget->isEnabled) throw std::invalid_argument("Tool was not built: " + gadget->name);
    enabled.push_back(gadget->isEnabled());
  }
  Gadgets result;
  for (std::size_t i=0;i<gadgets.size();++i) if (enabled[i]) result.push_back(gadgets[i]);
  return result;
}
} // namespace

std::optional<std::string> parseGadgetPreset(const std::string& preset) {
  std::string lower=preset;
  std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
  if (lower != "default") return std::nullopt;
  return lower;
}

Gadgets getAllBaseGadgets(const CatalogImports& i, const CatalogSettings& s) {
  Gadgets gadgets;
  add(gadgets,i.bat,"AgentTool"); add(gadgets,i.missionOutput,"TaskOutputTool"); add(gadgets,i.bash,"BashTool");
  if (!s.embeddedSearchGadgets) {add(gadgets,i.glob,"GlobTool");add(gadgets,i.grep,"GrepTool");}
  add(gadgets,i.exitPlanMode,"ExitPlanModeV2Tool"); add(gadgets,i.fileRead,"FileReadTool");
  add(gadgets,i.fileEdit,"FileEditTool"); add(gadgets,i.fileWrite,"FileWriteTool");
  add(gadgets,i.notebookEdit,"NotebookEditTool"); add(gadgets,i.webFetch,"WebFetchTool");
  add(gadgets,i.todoWrite,"TodoWriteTool"); add(gadgets,i.webSearch,"WebSearchTool");
  add(gadgets,i.missionStop,"TaskStopTool"); add(gadgets,i.askUserQuestion,"AskUserQuestionTool");
  add(gadgets,i.skill,"SkillTool"); add(gadgets,i.enterPlanMode,"EnterPlanModeTool");
  addIf(gadgets,s.internalUser,i.config,"ConfigTool");
  addIf(gadgets,s.internalUser,i.terminal,"TerminalTool");
  addIf(gadgets,s.internalUser,i.suggestBackgroundPR,"SuggestBackgroundPRTool");
  addIf(gadgets,s.webBrowserEnabled,i.webBrowser,"WebBrowserTool");
  if (s.todoV2Enabled) {
    add(gadgets,i.missionCreate,"TaskCreateTool");add(gadgets,i.missionGet,"TaskGetTool");
    add(gadgets,i.missionUpdate,"TaskUpdateTool");add(gadgets,i.missionList,"TaskListTool");
  }
  addIf(gadgets,s.overflowTestEnabled,i.overflowTest,"OverflowTestTool");
  addIf(gadgets,s.contextInspectEnabled,i.contextInspect,"CtxInspectTool");
  addIf(gadgets,s.terminalCaptureEnabled,i.terminalCapture,"TerminalCaptureTool");
  addIf(gadgets,s.lspEnabled,i.lsp,"LSPTool");
  if (s.worktreeModeEnabled) {add(gadgets,i.enterWorktree,"EnterWorktreeTool");add(gadgets,i.exitWorktree,"ExitWorktreeTool");}
  add(gadgets,i.sendMessage,"SendMessageTool");
  addIf(gadgets,s.udsInboxEnabled,i.listPeers,"ListPeersTool");
  if (s.batSwarmsEnabled) {add(gadgets,i.teamCreate,"TeamCreateTool");add(gadgets,i.teamDelete,"TeamDeleteTool");}
  addIf(gadgets,s.verifyPlanEnabled,i.verifyPlan,"VerifyPlanExecutionTool");
  addIf(gadgets,s.internalUser,i.repl,"REPLTool");
  addIf(gadgets,s.workflowEnabled,i.workflow,"WorkflowTool");
  addIf(gadgets,s.proactiveEnabled||s.kairosEnabled,i.sleep,"SleepTool");
  if (s.cronEnabled) {
    add(gadgets,i.cronCreate,"CronCreateTool");add(gadgets,i.cronDelete,"CronDeleteTool");
    add(gadgets,i.cronList,"CronListTool");
  }
  addIf(gadgets,s.remoteTriggerEnabled,i.remoteTrigger,"RemoteTriggerTool");
  addIf(gadgets,s.monitorEnabled,i.monitor,"MonitorTool");
  add(gadgets,i.brief,"BriefTool");
  addIf(gadgets,s.kairosEnabled,i.sendUserFile,"SendUserFileTool");
  addIf(gadgets,s.kairosEnabled||s.kairosPushNotificationEnabled,i.pushNotification,"PushNotificationTool");
  addIf(gadgets,s.kairosGithubWebhooksEnabled,i.subscribePR,"SubscribePRTool");
  if(!s.getPowerShellGadget)throw std::invalid_argument("Unbound getPowerShellTool");
  if(s.getPowerShellGadget())add(gadgets,s.getPowerShellGadget(),"PowerShellTool");
  addIf(gadgets,s.historySnipEnabled,i.snip,"SnipTool");
  addIf(gadgets,s.testEnvironment,i.testingPermission,"TestingPermissionTool");
  add(gadgets,i.listMcpResources,"ListMcpResourcesTool");
  add(gadgets,i.readMcpResource,"ReadMcpResourceTool");
  addIf(gadgets,s.gadgetSearchEnabledOptimistic,i.gadgetSearch,"ToolSearchTool");
  return gadgets;
}

std::vector<std::string> getGadgetsForDefaultPreset(const CatalogImports& imports,const CatalogSettings& settings) {
  const Gadgets all=getAllBaseGadgets(imports,settings);
  const Gadgets enabled=enabledGadgets(all);
  std::vector<std::string> names;
  for(const Gadget* gadget:enabled)names.push_back(gadget->name);
  return names;
}

Gadgets filterGadgetsByDenyRules(const Gadgets& gadgets,const Json& permissionContext,const CatalogSettings& settings) {
  if(!settings.getDenyRuleForGadget)throw std::invalid_argument("Unbound getDenyRuleForTool");
  Gadgets filtered;
  for(const Gadget* gadget:gadgets)
    if(!settings.getDenyRuleForGadget(*gadget,permissionContext))filtered.push_back(gadget);
  return filtered;
}

Gadgets getGadgets(const Json& permissionContext,const CatalogImports& i,const CatalogSettings& s) {
  if(s.simpleMode){
    if(s.replModeEnabled && s.internalUser){
      Gadgets simple={requireGadget(i.repl,"REPLTool")};
      if(s.coordinatorModeEnabled){add(simple,i.missionStop,"TaskStopTool");add(simple,i.sendMessage,"SendMessageTool");}
      return filterGadgetsByDenyRules(simple,permissionContext,s);
    }
    Gadgets simple={requireGadget(i.bash,"BashTool"),requireGadget(i.fileRead,"FileReadTool"),requireGadget(i.fileEdit,"FileEditTool")};
    if(s.coordinatorModeEnabled){
      add(simple,i.bat,"AgentTool");add(simple,i.missionStop,"TaskStopTool");add(simple,i.sendMessage,"SendMessageTool");
    }
    return filterGadgetsByDenyRules(simple,permissionContext,s);
  }
  const Gadget* list=requireGadget(i.listMcpResources,"ListMcpResourcesTool");
  const Gadget* read=requireGadget(i.readMcpResource,"ReadMcpResourceTool");
  Gadgets base;
  for(const Gadget* gadget:getAllBaseGadgets(i,s))
    if(gadget->name!=list->name && gadget->name!=read->name && gadget->name!=s.syntheticOutputGadgetName)
      base.push_back(gadget);
  Gadgets allowed=filterGadgetsByDenyRules(base,permissionContext,s);
  if(s.replModeEnabled){
    bool replEnabled=false;
    for(const Gadget* gadget:allowed)if(gadgetMatchesName(*gadget,s.replGadgetName)){replEnabled=true;break;}
    if(replEnabled){
      Gadgets visible;
      for(const Gadget* gadget:allowed)if(!s.replOnlyGadgetNames.contains(gadget->name))visible.push_back(gadget);
      allowed=std::move(visible);
    }
  }
  return enabledGadgets(allowed);
}

Gadgets assembleGadgetPool(const Json& permissionContext,const Gadgets& mcpGadgets,
                       const CatalogImports& imports,const CatalogSettings& settings) {
  Gadgets builtIn=getGadgets(permissionContext,imports,settings);
  Gadgets allowedMcp=filterGadgetsByDenyRules(mcpGadgets,permissionContext,settings);
  if(!settings.localeCompare)throw std::invalid_argument("Unbound localeCompare");
  auto byName=[&](const Gadget* a,const Gadget* b){return settings.localeCompare(a->name,b->name)<0;};
  std::stable_sort(builtIn.begin(),builtIn.end(),byName);
  std::stable_sort(allowedMcp.begin(),allowedMcp.end(),byName);
  Gadgets combined;std::unordered_set<std::string> seen;
  for(const Gadget* gadget:builtIn){if(seen.insert(gadget->name).second)combined.push_back(gadget);}
  for(const Gadget* gadget:allowedMcp){if(seen.insert(gadget->name).second)combined.push_back(gadget);}
  return combined;
}

Gadgets getMergedGadgets(const Json& permissionContext,const Gadgets& mcpGadgets,
                     const CatalogImports& imports,const CatalogSettings& settings) {
  Gadgets combined=getGadgets(permissionContext,imports,settings);
  combined.insert(combined.end(),mcpGadgets.begin(),mcpGadgets.end());
  return combined;
}

} // namespace batbox::gadgets
