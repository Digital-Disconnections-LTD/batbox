#pragma once
#include "application/gadgets/ApplicationGadgets.hpp"
#include "application/connectors/Bootstrap.hpp"
#include "application/model/LocalModelService.hpp"
#include "application/permissions/PermissionProvider.hpp"
#include "application/shell/ApplicationShell.hpp"
#include "application/Config.hpp"
#include "app_state/State.hpp"
#include "application/file_history/History.hpp"
#include "application/lsp/Runtime.hpp"
#include "missions/store/Gadgets.hpp"

namespace batbox::application::gadgets {
struct DefaultInputs {
  event_loop::Loop& owner;
  explicit DefaultInputs(event_loop::Loop& loop):owner(loop){}
  application::Config config;
  std::shared_ptr<app_state::AppStateStore> store;
  std::shared_ptr<messages::Services> messages;
  std::shared_ptr<file_state_stash::FileStateStash> fileStash;
  std::shared_ptr<application::permissions::PermissionProvider> permission;
  std::shared_ptr<application::connectors::Bundle> connectors;
  std::shared_ptr<application::model::LocalModelService> model;
  std::shared_ptr<application::file_history::History> fileHistory;
  std::shared_ptr<application::lsp::Runtime> lsp;
  application::shell::Host shell;
  gadget_results::StorageImports storage;
  batbox::gadgets::live_roster::BatFactory bat;
  std::function<batbox::gadgets::Gadget()> makeBatGadget;
  std::vector<batbox::gadgets::Gadget> additionalGadgets;
  std::function<void(std::exception_ptr)> onError;
  std::function<void(const std::string&)> log;
  std::function<void(const Json&)> showBackgroundHint;
  std::function<std::string()> missionListId;
  std::function<std::vector<std::string>(const std::string&,const Json&,
    const batbox::gadgets::GadgetCallContext&)> missionLifecycleHooks;
  // True while the user has poetry queued for the main Flight.
  std::function<bool()> userMessageQueued;
  std::shared_ptr<batbox::skills::SkillRoster> skillRoster;
  batbox::skills::SkillRosterServices skillServices;
};
// What the built-in file and shell Gadgets show in their cards: header text
// after the Gadget name and result views (interactive/gadget_views). Options
// carry "verbose" or "transcriptMode" when the transcript wants full output.
Json fileGadgetUseView(const std::string& name,const Json& input,
  const std::string& cwd,const std::string& home);
Json fileGadgetResultView(const std::string& name,const Json& output,
  const Json& options,const std::string& cwd,const std::string& home);
Json shellGadgetResultView(const Json& output,const Json& options);
// settings.json and settings.local.json inside a .batbox directory; edits to
// them must still match the settings schema.
bool isSettingsFile(const std::string& file);
// Complete local gadget imports for an interactive owner. Enabled optional
// integrations without indigenous providers remain explicit feature boundaries.
Inputs makeDefaultInputs(DefaultInputs defaults);
} // namespace batbox::application::gadgets
