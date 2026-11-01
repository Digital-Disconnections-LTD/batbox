#pragma once
#include "application_gadgets/ApplicationGadgets.hpp"
#include "application_connectors/Bootstrap.hpp"
#include "application_model/LocalModelService.hpp"
#include "application_permissions/PermissionProvider.hpp"
#include "application_shell/ApplicationShell.hpp"
#include "application/Config.hpp"
#include "app_state/State.hpp"
#include "application_file_history/History.hpp"
#include "application_lsp/Runtime.hpp"
#include "gadgets/mission_management/Gadgets.hpp"

namespace batbox::application_gadgets {
struct DefaultInputs {
  event_loop::Loop& owner;
  explicit DefaultInputs(event_loop::Loop& loop):owner(loop){}
  application::Config config;
  std::shared_ptr<app_state::AppStateStore> store;
  std::shared_ptr<messages::Services> messages;
  std::shared_ptr<file_state_stash::FileStateStash> fileStash;
  std::shared_ptr<application_permissions::PermissionProvider> permission;
  std::shared_ptr<application_connectors::Bundle> connectors;
  std::shared_ptr<application_model::LocalModelService> model;
  std::shared_ptr<application_file_history::History> fileHistory;
  std::shared_ptr<application_lsp::Runtime> lsp;
  application_shell::Host shell;
  gadget_results::StorageImports storage;
  gadgets::live_roster::BatFactory bat;
  std::function<gadgets::Gadget()> makeBatGadget;
  std::vector<gadgets::Gadget> additionalGadgets;
  std::function<void(std::exception_ptr)> onError;
  std::function<void(const std::string&)> log;
  std::function<void(const Json&)> showBackgroundHint;
  std::function<std::string()> missionListId;
  std::function<std::vector<std::string>(const std::string&,const Json&,
    const gadgets::GadgetCallContext&)> missionLifecycleHooks;
  std::function<void()> expandMissionList;
  std::shared_ptr<skills::SkillRoster> skillRoster;
  skills::SkillRosterServices skillServices;
};
// What the built-in file and shell Gadgets show in their cards: header text
// after the Gadget name and result views (interactive/gadget_views). Options
// carry "verbose" or "transcriptMode" when the transcript wants full output.
Json fileGadgetUseView(const std::string& name,const Json& input,
  const std::string& cwd,const std::string& home);
Json fileGadgetResultView(const std::string& name,const Json& output,
  const Json& options,const std::string& cwd,const std::string& home);
Json shellGadgetResultView(const Json& output,const Json& options);
// Complete local gadget imports for an interactive owner. Enabled optional
// integrations without indigenous providers remain explicit feature boundaries.
Inputs makeDefaultInputs(DefaultInputs defaults);
} // namespace batbox::application_gadgets
