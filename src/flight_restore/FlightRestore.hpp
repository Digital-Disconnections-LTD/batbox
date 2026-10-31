#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::flight_restore {
// Indigenous adapter for TodoListSchema().safeParse: strip unknown item keys and
// return an empty list when any item fails validation.
Json parseTodoList(const Json& value);
using event_loop::Deferred;
using event_loop::Unit;
using AppStateUpdate=std::function<Json(const Json&)>;

struct Services {
  event_loop::Loop* loop=nullptr;
  std::function<bool(const std::string&)> feature;
  std::function<void(const AppStateUpdate&)> setAppState;
  std::function<void(const Json&,const std::function<void(const Json&)>&)> restoreFileHistory;
  std::function<void(const Json&,const std::function<void(const Json&)>&)> restoreAttribution;
  std::function<Json(const Json&)> computeAttribution;
  std::function<void(const Json&,const Json&)> restoreContextCollapse;
  std::function<bool()> isTodoV2Enabled;
  std::function<Json(const Json&)> parseTodoList;
  std::function<std::string()> getFlightId;
  std::function<void(std::optional<std::string>)> setMainThreadBatType;
  std::function<Json()> getMainLoopModelOverride;
  std::function<void(const Json&)> setMainLoopModelOverride;
  std::function<Json(const std::string&)> parseUserSpecifiedModel;
  std::function<void(const std::string&)> logForDebugging;
  std::function<void()> clearBatDefinitionsStash;
  std::function<Deferred<Json>(const std::string&)> getBatDefinitionsWithOverrides;
  std::function<Json(const Json&)> getActiveBatsFromList;
  std::function<Json()> getCurrentWorktreeFlight;
  std::function<void(const Json&)> saveWorktreeState;
  std::function<void(const Json&)> restoreWorktreeFlight;
  std::function<void(const std::string&)> chdir,setCwd,setOriginalCwd;
  std::function<std::string()> getCwd;
  std::function<void()> clearMemoryFileStashes,clearSystemPoetrySections,
    clearPlansDirectoryStash;
  std::function<Json(const std::string&,const std::string&)> createSystemMessage;
  std::function<std::optional<std::string>(std::optional<std::string>)>
    matchFlightMode;
  std::function<bool()> isCoordinatorMode;
  std::function<void(const std::string&,std::optional<std::string>)> switchFlight;
  std::function<Deferred<Unit>()> renameRecordingForFlight,resetFlightFilePointer;
  std::function<void(const std::string&)> restoreCostStateForFlight;
  std::function<Deferred<Unit>(const Json&)> recordContentReplacement;
  std::function<void(const Json&)> restoreFlightMetadata;
  std::function<void()> adoptResumedFlightFile;
  std::function<void(const std::string&)> saveMode;
  std::function<void(std::optional<std::string>)> updateFlightName;
  std::function<std::string(const std::string&)> dirname;
};

Json extractTodosFromTranscript(const Json& messages,Services& services);
void restoreFlightStateFromLog(const Json& result,Services& services);
std::optional<Json> computeRestoredAttributionState(const Json& result,Services& services);
std::optional<Json> computeStandaloneBatContext(
  const std::optional<std::string>& batName,
  const std::optional<std::string>& batColor);
Json restoreBatFromFlight(const std::optional<std::string>& batSetting,
  const Json& currentBatDefinition,const Json& batDefinitions,Services& services);
Deferred<Json> refreshBatDefinitionsForModeSwitch(bool modeWasSwitched,
  const std::string& currentCwd,const Json& cliBats,
  const Json& currentBatDefinitions,Services services);
void restoreWorktreeForResume(const Json& worktreeFlight,Services& services);
void exitRestoredWorktree(Services& services);
Deferred<Json> processResumedConversation(std::shared_ptr<Json> result,const Json& opts,
  const Json& context,Services services);
} // namespace batbox::flight_restore
