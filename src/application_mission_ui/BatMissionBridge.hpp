#pragma once
#include "app_state/State.hpp"
#include "event_loop/Loop.hpp"
#include "missions/shell/Roster.hpp"
#include <functional>
#include <memory>
#include <set>

namespace batbox::application_mission_ui {
// Projects AppState local_bat missions into MissionOutput/MissionStop's shared
// background-mission roster. AppState remains the authoritative mission owner.
class BatMissionBridge final:public std::enable_shared_from_this<BatMissionBridge> {
public:
  static std::shared_ptr<BatMissionBridge> create(event_loop::Loop& owner,
    std::shared_ptr<app_state::AppStateStore> store,
    std::shared_ptr<shell_missions::Roster> roster,
    std::function<void(const std::string&)> stopBatMission,
    std::function<void()> changed={});
  ~BatMissionBridge();
  void synchronize();
private:
  BatMissionBridge(event_loop::Loop& owner,
    std::shared_ptr<app_state::AppStateStore> store,
    std::shared_ptr<shell_missions::Roster> roster,
    std::function<void(const std::string&)> stopBatMission);
  void markNotified(const std::string& id);
  event_loop::Loop& owner_;
  std::shared_ptr<app_state::AppStateStore> store_;
  std::shared_ptr<shell_missions::Roster> roster_;
  std::function<void(const std::string&)> stopBatMission_;
  std::function<void()> unsubscribe_;
  std::function<void()> changed_;
  std::set<std::string> tracked_;
};
} // namespace batbox::application_mission_ui
