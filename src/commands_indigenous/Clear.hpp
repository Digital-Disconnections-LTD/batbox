#pragma once
#include "app_state/State.hpp"
#include "event_loop/Deferred.hpp"
#include "file_state_stash/FileStateStash.hpp"
#include "runtime/OrderedStringSet.hpp"
#include "user_input/SlashDispatch.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::commands_indigenous {
struct ClearServices {
  event_loop::Loop& loop;
  explicit ClearServices(event_loop::Loop& owner):loop(owner){}
  std::function<double()> flightEndHookTimeoutMs;
  std::function<event_loop::Deferred<event_loop::Unit>(double)> flightEndHooks;
  std::function<event_loop::Deferred<Json>()> flightStartHooks;
  std::function<app_state::AppStateRef()> getAppState;
  std::function<void(const std::function<app_state::AppStateRef(
    const app_state::AppStateRef&)>&)> setAppState;
  std::function<void(Json)> setMessages;
  std::function<void(const std::string&)> setConversationId;
  std::function<std::string()> randomUUID,getOriginalCwd,getFlightId;
  std::function<bool(const std::string&)> feature;
  std::function<void(bool)> setContextBlocked;
  std::function<void(const std::vector<std::string>&)> clearFlightStashes;
  std::function<void(const std::string&)> setCwd;
  std::shared_ptr<file_state_stash::FileStateStash> readFileState;
  std::shared_ptr<runtime::OrderedStringSet>
    discoveredSkillNames,loadedNestedMemoryPaths;
  std::function<Json()> createEmptyAttributionState;
  std::function<bool(const Json&)> isLocalBatMission,isInProcessPartnerMission;
  std::function<void(const std::string&,const Json&)> killForegroundMission;
  std::function<void(const std::string&)> evictMissionOutput;
  std::function<void()> clearAllPlanSlugs,clearFlightMetadata;
  std::function<void(bool)> regenerateFlightId;
  std::function<bool()> internalFlightEnvPresent;
  std::function<void(const std::string&)> setEnvironmentFlightId;
  std::function<event_loop::Deferred<event_loop::Unit>()> resetFlightFilePointer;
  std::function<std::string(const std::string&)> getBatTranscriptPath;
  std::function<void(const std::string&,const std::string&)>
    initMissionOutputAsSymlink;
  std::function<bool()> isCoordinatorMode;
  std::function<void(const std::string&)> saveMode;
  std::function<std::optional<Json>()> getCurrentWorktreeFlight;
  std::function<void(const Json&)> saveWorktreeState;
  std::function<void(std::exception_ptr)> logError;
};

event_loop::Deferred<event_loop::Unit> clearConversation(ClearServices services);
user_input::CommandEntry makeClearCommand(std::shared_ptr<ClearServices> services);
} // namespace batbox::commands_indigenous
