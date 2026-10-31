#pragma once
#include "app_state/State.hpp"
#include "shell/ManagedShell.hpp"
#include "missions/shell/Roster.hpp"
#include "user_input/UiCommandRouter.hpp"
#include "interactive/conversation_shell/Shell.hpp"
#include "gadgets/mission_management/Store.hpp"
#include <functional>
#include <memory>

namespace batbox::application_mission_ui {
// Indigenous view services for the background-mission dialog. The mission list is
// read live on the owner loop; shell output and stops run off that loop.
struct Inputs {
  event_loop::Loop& owner;
  std::shared_ptr<app_state::AppStateStore> store;
  std::shared_ptr<shell_missions::Roster> shellMissions;
  std::shared_ptr<shell::ManagedShell> managedShell;
  std::shared_ptr<mission_management::Store> missionStore;
  std::function<std::string()> missionListId;
  std::function<void(const std::string&)> stopBatMission;
  std::function<void(const std::string&)> openBatConversation;
  std::function<Json()> batDefinitions;
  std::function<void(const std::string&)> dismissBatMission;
  std::function<void()> reloadBatDefinitions;
  std::function<std::shared_ptr<interactive::conversation_shell::Shell>()> getShell;
  bool batsOnly=false,includeCompletedBats=false;
  std::function<int()> terminalRows;
  std::function<void(std::exception_ptr)> onError;
  explicit Inputs(event_loop::Loop& loop):owner(loop){}
};
user_input::CommandEntry makeMissionsCommand(
  const std::shared_ptr<user_input::UiCommandRouter>& router,Inputs inputs);
user_input::CommandEntry makeBatsCommand(
  const std::shared_ptr<user_input::UiCommandRouter>& router,Inputs inputs);
} // namespace batbox::application_mission_ui
