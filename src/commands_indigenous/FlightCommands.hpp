#pragma once
#include "commands_indigenous/UiCommands.hpp"
#include "monster/Monster.hpp"
#include <memory>

namespace batbox::commands_indigenous {
struct RenameServices {
  event_loop::Loop& loop;
  explicit RenameServices(event_loop::Loop& owner):loop(owner){}
  std::function<bool()> isPartner;
  std::function<Json()> getMessages;
  std::function<Json(const Json&)> afterCompactBoundary;
  std::function<event_loop::Deferred<std::optional<std::string>>(const Json&)>
    generateFlightName;
  std::function<std::string()> flightId,transcriptPath;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::string&,const std::string&,const std::string&)>
    saveCustomTitle,saveBatName;
  std::function<std::optional<std::string>()> bridgeFlightId;
  std::function<void(const std::string&,const std::string&)>
    scheduleBridgeTitleUpdate;
  std::function<void(const std::string&)> setStandaloneBatName;
};
struct CostServices {
  event_loop::Loop& loop;
  explicit CostServices(event_loop::Loop& owner):loop(owner){}
  std::function<bool()> isSubscriber,isUsingOverage,showCostForInternalUser;
  std::function<std::string()> formatTotalCost;
};
struct ExitServices {
  event_loop::Loop& loop;
  explicit ExitServices(event_loop::Loop& owner):loop(owner){}
  std::function<bool()> backgroundFlightsEnabled,isBackgroundFlight,
    hasCurrentWorktree;
  std::function<void()> detachBackgroundFlight;
  std::function<std::string()> randomGoodbye;
  std::function<event_loop::Deferred<event_loop::Unit>(int,
    const std::string&)> gracefulShutdown;
  std::function<interactive::CommandUi::Invoke()> worktreeExitFlow;
};
struct RewindServices {
  event_loop::Loop& loop;
  explicit RewindServices(event_loop::Loop& owner):loop(owner){}
  std::function<void()> openMessageSelector;
};
struct MonsterServices {
  std::shared_ptr<monster::Roller> roller;
};

user_input::CommandEntry makeRenameCommand(
  std::shared_ptr<user_input::UiCommandRouter> router,
  std::shared_ptr<RenameServices> services);
user_input::CommandEntry makeCostCommand(std::shared_ptr<CostServices> services);
user_input::CommandEntry makeExitCommand(
  std::shared_ptr<user_input::UiCommandRouter> router,
  std::shared_ptr<ExitServices> services);
user_input::CommandEntry makeRewindCommand(
  std::shared_ptr<RewindServices> services);
user_input::CommandEntry makeMonsterCommand(
  std::shared_ptr<user_input::UiCommandRouter> router,
  std::shared_ptr<MonsterServices> services);
} // namespace batbox::commands_indigenous
