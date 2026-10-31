#pragma once
#include "gadgets/Gadget.hpp"
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace batbox::gadgets {

// Every pointer is a real gadget module import. A selected, unbound import is
// reported rather than replaced with an executable placeholder.
struct CatalogImports {
  const Gadget *bat=nullptr, *missionOutput=nullptr, *bash=nullptr, *glob=nullptr, *grep=nullptr;
  const Gadget *exitPlanMode=nullptr, *fileRead=nullptr, *fileEdit=nullptr, *fileWrite=nullptr;
  const Gadget *notebookEdit=nullptr, *webFetch=nullptr, *todoWrite=nullptr, *webSearch=nullptr;
  const Gadget *missionStop=nullptr, *askUserQuestion=nullptr, *skill=nullptr, *enterPlanMode=nullptr;
  const Gadget *config=nullptr, *terminal=nullptr, *suggestBackgroundPR=nullptr;
  const Gadget *webBrowser=nullptr, *missionCreate=nullptr, *missionGet=nullptr, *missionUpdate=nullptr, *missionList=nullptr;
  const Gadget *overflowTest=nullptr, *contextInspect=nullptr, *terminalCapture=nullptr, *lsp=nullptr;
  const Gadget *enterWorktree=nullptr, *exitWorktree=nullptr, *sendMessage=nullptr, *listPeers=nullptr;
  const Gadget *teamCreate=nullptr, *teamDelete=nullptr, *verifyPlan=nullptr, *repl=nullptr;
  const Gadget *workflow=nullptr, *sleep=nullptr, *cronCreate=nullptr, *cronDelete=nullptr, *cronList=nullptr;
  const Gadget *remoteTrigger=nullptr, *monitor=nullptr, *brief=nullptr, *sendUserFile=nullptr;
  const Gadget *pushNotification=nullptr, *subscribePR=nullptr, *powerShell=nullptr, *snip=nullptr;
  const Gadget *testingPermission=nullptr, *listMcpResources=nullptr, *readMcpResource=nullptr;
  const Gadget *gadgetSearch=nullptr;
};

struct CatalogSettings {
  bool embeddedSearchGadgets=false;
  bool internalUser=false;
  bool todoV2Enabled=false;
  bool lspEnabled=false;
  bool worktreeModeEnabled=false;
  bool batSwarmsEnabled=false;
  bool simpleMode=false;
  bool replModeEnabled=false;
  bool coordinatorModeEnabled=false;
  bool gadgetSearchEnabledOptimistic=false;
  bool testEnvironment=false;
  bool powerShellEnabled=false;
  bool cronEnabled=false;
  bool remoteTriggerEnabled=false;
  bool monitorEnabled=false;
  bool webBrowserEnabled=false;
  bool workflowEnabled=false;
  bool historySnipEnabled=false;
  bool overflowTestEnabled=false;
  bool contextInspectEnabled=false;
  bool terminalCaptureEnabled=false;
  bool proactiveEnabled=false;
  bool kairosEnabled=false;
  bool kairosPushNotificationEnabled=false;
  bool kairosGithubWebhooksEnabled=false;
  bool verifyPlanEnabled=false;
  bool udsInboxEnabled=false;
  std::string syntheticOutputGadgetName;
  std::string replGadgetName;
  std::set<std::string> replOnlyGadgetNames;
  std::function<bool(const Gadget&, const Json&)> getDenyRuleForGadget;
  std::function<int(const std::string&, const std::string&)> localeCompare;
  std::function<const Gadget*()> getPowerShellGadget;
};

std::optional<std::string> parseGadgetPreset(const std::string& preset);
Gadgets getAllBaseGadgets(const CatalogImports& imports, const CatalogSettings& settings);
std::vector<std::string> getGadgetsForDefaultPreset(const CatalogImports& imports, const CatalogSettings& settings);
Gadgets filterGadgetsByDenyRules(const Gadgets& gadgets, const Json& permissionContext, const CatalogSettings& settings);
Gadgets getGadgets(const Json& permissionContext, const CatalogImports& imports, const CatalogSettings& settings);
Gadgets assembleGadgetPool(const Json& permissionContext, const Gadgets& mcpGadgets,
                       const CatalogImports& imports, const CatalogSettings& settings);
Gadgets getMergedGadgets(const Json& permissionContext, const Gadgets& mcpGadgets,
                     const CatalogImports& imports, const CatalogSettings& settings);

} // namespace batbox::gadgets
