#pragma once
#include "shell/ManagedShell.hpp"
#include "gadgets/Gadget.hpp"
#include "gadgets/bash/Helpers.hpp"
#include "gadgets/bash/Hints.hpp"
#include "gadgets/bash/CwdPolicy.hpp"
#include "shell/MissionOutputPoller.hpp"
#include <memory>
#include <stdexcept>

namespace batbox::bash_gadget {
class ShellError final:public std::runtime_error {
public:
  ShellError(std::string stdoutText,std::string stderrText,int code,bool interrupted);
  std::string stdoutText,stderrText;
  int code;
  bool interrupted;
};
struct Services {
  std::shared_ptr<shell::ManagedShell> shell;
  std::shared_ptr<shell::MissionOutputPoller> missionOutputPoller;
  SplitCommand splitCommand;
  SplitCommand splitWithOperators;
  std::function<Json(const Json&)> parseInput;
  Json inputJSONSchema;
  Json outputJSONSchema;
  std::function<std::future<std::string>(const gadgets::PoetryContext&)> poetry;
  std::function<std::future<Json>(const Json&,const Json&)> checkPermissions;
  std::function<bool(const Json&,bool)> readOnlyAllowed;
  std::function<bool(const std::string&)> commandHasAnyCd;
  std::function<std::future<std::function<bool(const std::string&)>>(const Json&)> preparePermissionMatcher;
  std::function<bool(const Json&)> shouldUseSandbox;
  std::function<std::string(const std::string&,const Json&,const Json&)> sandboxCommand;
  std::function<std::string()> getSandboxTmpDir;
  std::function<void(const std::string&)> logDebug;
  std::function<std::string(const std::string&,const std::string&)> annotateSandboxFailures;
  std::function<bool(const Json&)> resetCwdIfOutsideProject;
  std::function<Json(const std::string&,const std::string&)> extractHints;
  std::function<void(const Json&)> recordHint;
  std::function<std::optional<std::string>(const std::string&,const shell::ExecResult&)> resizeImage;
  std::function<gadgets::GadgetResult(const Json&,const gadgets::GadgetCallContext&,const Json&)> applySedEdit;
  std::function<Json(const std::string&)> parseSedEdit;
  std::function<std::string(const Json&)> fileEditUserFacingName;
  std::function<std::string(const std::string&,std::size_t)> truncateSummary;
  std::function<std::string(const std::string&)> getGadgetResultPath;
  std::function<void(const std::string&,const std::string&,const std::string&,const Json&)> registerBackgroundMission;
  std::function<void(const std::string&,const std::string&,const std::string&,const Json&)> registerForegroundMission;
  std::function<void(const std::string&)> unregisterForegroundMission;
  std::function<bool(const std::string&)> backgroundExistingForegroundMission;
  std::function<void(const std::string&)> markMissionNotified;
  std::function<void(const Json&)> showBackgroundHint;
  std::function<void(const Json&)> clearGadgetJSX;
  std::function<Json(const Json&,const Json&)> renderUse;
  std::function<Json(const Json&,const Json&,const Json&)> renderResult;
  std::function<Json(const Json&,const Json&)> renderProgress;
  std::function<Json()> renderQueued;
  std::function<Json(const Json&,const Json&)> renderError;
  bool monitorGadgetEnabled=false;
  bool backgroundMissionsDisabled=false;
  bool sandboxIndicatorEnabled=false;
  bool assistantAutoBackgroundEnabled=false;
  std::size_t gadgetSummaryMaxLength=50;
  double assistantBlockingBudgetMs=15000;
  std::string originalCwd;
};
Json mapGadgetResult(const Json& output,const std::string& gadgetUseID,
  const std::function<std::string(const std::string&)>& getMissionOutputPath,
  double assistantBlockingBudgetMs);
gadgets::Gadget makeBashGadget(Services services);
void bindIndigenousCommandSplitters(Services& services);
void bindIndigenousBashSchema(Services& services, double maxTimeoutMs);
void bindIndigenousHintParser(Services& services, HintProtocol protocol);
void bindIndigenousCwdPolicy(Services& services, CwdPolicyServices policy);
} // namespace batbox::bash_gadget
