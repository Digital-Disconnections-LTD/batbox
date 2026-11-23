#pragma once
#include "application/Config.hpp"
#include "application/compaction_settings/Policy.hpp"
#include "application/model/LocalModelService.hpp"
#include "application/engine/Compactor.hpp"
#include "application/poetry/Provider.hpp"
#include "application/flights/Flight.hpp"
#include "application/hooks/Provider.hpp"
#include "application/permissions/PermissionProvider.hpp"
#include "application/shell/ApplicationShell.hpp"
#include "messages/Messages.hpp"
#include "runtime/OrderedStringSet.hpp"
#include "commands_indigenous/Clear.hpp"
#include "commands_indigenous/Compact.hpp"
#include "commands_indigenous/UiCommands.hpp"
#include "interactive/conversation_shell/Shell.hpp"
#include "interactive/presentation_providers/ThemeState.hpp"
#include "application/runtime/ApplicationRuntime.hpp"
#include "mcp/manager/IndigenousRuntime.hpp"
#include "bootstrap/InvokedSkills.hpp"
#include "application/skills/Provider.hpp"
#include "application/memory/Provider.hpp"
#include "bootstrap/FlightTransitions.hpp"

namespace batbox::application::commands {
struct McpAuthUi {
  std::string server,url,callbackText;
  std::function<void(const std::string&)> submitCallback;
  std::function<void()> changed;
};
struct Inputs {
  event_loop::Loop& owner;
  application::Config config;
  std::shared_ptr<app_state::AppStateStore> store;
  std::shared_ptr<application::compaction_settings::Policy> compactionPolicy;
  interactive::PresentationState& view;
  std::shared_ptr<application::model::LocalModelService> model;
  std::shared_ptr<application::poetry::Provider> poetry;
  std::shared_ptr<application::flights::Flight> flight;
  std::shared_ptr<application::engine::Compactor> compactor;
  std::shared_ptr<mcp::manager::IndigenousRuntime> mcpRuntime;
  std::shared_ptr<McpAuthUi> mcpAuthUi;
  std::shared_ptr<bootstrap::InvokedSkillsState> invokedSkills;
  std::shared_ptr<application::skills::Provider> skills;
  std::shared_ptr<application::memory::Provider> memory;
  mcp::ResultServices mcpPoetryResults;
  std::function<event_loop::Deferred<Json>(const std::string&)>
    mcpPoetryAttachments;
  std::function<std::shared_ptr<batbox::bats::BatContext>()> getBatContext;
  std::shared_ptr<application::hooks::Provider> hooks;
  std::shared_ptr<application::permissions::PermissionProvider> permission;
  std::shared_ptr<bootstrap::FlightTransitions> flightTransitions;
  std::shared_ptr<messages::Services> messages;
  std::shared_ptr<file_state_stash::FileStateStash> fileStash;
  application::shell::Host managedShell;
  std::shared_ptr<batbox::runtime::OrderedStringSet> discoveredSkills,loadedMemoryPaths;
  std::function<std::string(const std::string&)> batTranscriptPath;
  std::function<void(const std::string&,const Json&)> killForegroundMission;
  std::function<void(const std::string&)> stopBatMission;
  std::function<bool(const std::string&)> backgroundBatMission;
  std::function<void(const std::string&)> openBatConversation,dismissBatMission;
  std::function<Json()> batDefinitions;
  std::function<void()> reloadBatDefinitions;
  std::function<std::shared_ptr<application::runtime::ApplicationRuntime>()> getRuntime;
  std::function<std::shared_ptr<interactive::conversation_shell::Shell>()> getShell;
  std::function<std::shared_ptr<interactive::presentation_providers::ThemeState>()> theme;
  std::function<void()> openRewind;
  // Shows the system result of a command opened by the application itself.
  std::function<void(const std::string&)> showNotice;
  std::function<void(std::vector<std::string>)> updateWatchPaths;
  std::function<void(const std::string&)> log;
  std::function<void(std::exception_ptr)> onError;
  std::function<void()> requestExit;
  std::function<Json()> runtimeMemory;
  std::shared_ptr<commands_indigenous::ClearServices> clear;
  std::shared_ptr<commands_indigenous::CompactServices> compact;
  std::shared_ptr<commands_indigenous::ResumeServices> resume;
  explicit Inputs(event_loop::Loop& loop,interactive::PresentationState& state):
    owner(loop),view(state){}
};
class Bundle final:public std::enable_shared_from_this<Bundle> {
public:
  static std::shared_ptr<Bundle> create(Inputs inputs);
  const std::vector<user_input::CommandEntry>& entries()const{return entries_;}
  std::shared_ptr<user_input::UiCommandRouter> router()const{return router_;}
  bool backgroundForegroundMissions();
  void openMissions(const std::string& id={});
  bool openUiCommand(const std::string& name,const std::string& args={});
private:
  explicit Bundle(Inputs inputs):inputs_(std::move(inputs)){}
  void initialize();
  Inputs inputs_;
  std::shared_ptr<user_input::UiCommandRouter> router_;
  std::vector<user_input::CommandEntry> entries_;
};
} // namespace batbox::application::commands
