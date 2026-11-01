#pragma once
#include "user_input/UiCommandRouter.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::commands_indigenous {
struct ModelState {
  std::optional<std::string> baseModel,flightModel,effort;
  bool fastMode=false;
};
struct ModelChoice {
  std::string label;
  std::optional<std::string> model,effort;
};
struct ModelServices {
  event_loop::Loop& loop;
  explicit ModelServices(event_loop::Loop& owner):loop(owner){}
  std::function<ModelState()> getState;
  std::function<void(std::optional<std::string>)> setModel;
  std::function<void(bool)> setFastMode;
  std::function<std::string(const std::string&)> renderDefaultModelSetting;
  std::function<std::string(const std::string&)> renderModelName;
  std::function<std::string()> mainLoopModel;
  std::function<std::string()> defaultModelSetting;
  std::function<std::vector<ModelChoice>()> choices;
  std::function<bool(const std::string&)> isModelAllowed,
    premiumExtendedContextUnavailable,standardExtendedContextUnavailable,
    isKnownAlias;
  std::function<event_loop::Deferred<Json>(const std::string&)> validateModel;
  std::function<bool()> isFastModeEnabled,isFastModeAvailable,
    isExtendedContextMergeEnabled;
  std::function<bool(const std::optional<std::string>&)>
    isFastModeSupportedByModel;
  std::function<bool(const std::optional<std::string>&,bool,bool)>
    billedAsExtraUsage;
  std::function<void()> clearFastModeCooldown;
  std::function<bool()> immediate;
  std::function<std::string(const std::string&)> bold;
};

struct CrossProject {
  bool isCrossProject=false,isSameRepoWorktree=false;
  // The live workspace can be rebound to the Flight's project directory, so
  // the picker resumes in this process instead of printing a cd command.
  bool canSwitchInProcess=false;
  std::string command,projectPath;
};
struct ResumeServices {
  event_loop::Loop& loop;
  explicit ResumeServices(event_loop::Loop& owner):loop(owner){}
  std::function<std::string()> originalCwd,currentFlightId;
  std::function<event_loop::Deferred<std::vector<std::string>>(
    const std::string&)> worktreePaths;
  std::function<event_loop::Deferred<Json>(const std::vector<std::string>&)>
    loadSameRepoLogs;
  std::function<event_loop::Deferred<Json>()> loadAllProjectsLogs;
  std::function<std::string(const Json&)> flightIdFromLog,logLabel;
  std::function<bool(const Json&)> isLiteLog;
  std::function<event_loop::Deferred<Json>(const Json&)> loadFullLog;
  std::function<std::optional<std::string>(const std::string&)> validateUuid;
  std::function<double(const Json&)> modifiedTimeMs;
  std::function<event_loop::Deferred<std::optional<Json>>(
    const std::string&)> getLastFlightLog;
  std::function<bool()> customTitleEnabled;
  std::function<event_loop::Deferred<Json>(const std::string&,bool)>
    searchByCustomTitle;
  std::function<CrossProject(const Json&,bool,
    const std::vector<std::string>&)> checkCrossProject;
  std::function<event_loop::Deferred<std::optional<std::string>>(
    const std::string&)> setClipboard;
  std::function<void(const std::string&)> writeOutput;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::string&,const Json&,const std::string&)> resume;
  std::function<void(std::exception_ptr)> logError;
  std::function<std::string(const std::string&)> bold;
  // Optional picker and lookup refinements. Without them the picker filters
  // on row labels and argument lookup falls back to exact custom titles.
  std::function<std::string(const Json&)> searchText,projectLabel;
  std::function<std::vector<std::size_t>(const Json&,const std::string&)>
    matchLogs;
  // Newest resumable Flight for this directory and its sibling worktrees.
  std::function<event_loop::Deferred<std::optional<Json>>()> latestLog;
  // Picker row presentation under the logLabel row: a dim detail line and a
  // badge. Detail text is also matched by the filter.
  std::function<interactive::UiChoice(const Json&)> logChoice;
};

user_input::CommandEntry makeHelpCommand(
  std::shared_ptr<user_input::UiCommandRouter> router,
  std::function<std::vector<Json>()> commands);
user_input::CommandEntry makeModelCommand(
  std::shared_ptr<user_input::UiCommandRouter> router,
  std::shared_ptr<ModelServices> services);
user_input::CommandEntry makeResumeCommand(
  std::shared_ptr<user_input::UiCommandRouter> router,
  std::shared_ptr<ResumeServices> services);
user_input::CommandEntry makeContinueCommand(
  std::shared_ptr<user_input::UiCommandRouter> router,
  std::shared_ptr<ResumeServices> services);
// Picker filter primitives, exposed for tests.
std::string applyFilterKeys(std::string filter,const std::string& keys);
bool filterMatches(const std::string& haystack,const std::string& filter);
} // namespace batbox::commands_indigenous
