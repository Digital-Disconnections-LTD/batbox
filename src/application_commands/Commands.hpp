#pragma once
#include "application/Config.hpp"
#include "application_compaction_settings/Policy.hpp"
#include "application_model/LocalModelService.hpp"
#include "application_engine/Compactor.hpp"
#include "application_poetry/Provider.hpp"
#include "application_flights/Flight.hpp"
#include "application_hooks/Provider.hpp"
#include "application_permissions/PermissionProvider.hpp"
#include "application_shell/ApplicationShell.hpp"
#include "messages/Messages.hpp"
#include "runtime/OrderedStringSet.hpp"
#include "commands_indigenous/CoreCatalog.hpp"
#include "interactive/conversation_shell/Shell.hpp"
#include "interactive/presentation_providers/ThemeState.hpp"
#include "application_runtime/ApplicationRuntime.hpp"
#include "mcp/manager/IndigenousRuntime.hpp"
#include "bootstrap/InvokedSkills.hpp"
#include "application_skills/Provider.hpp"
#include "application_memory/Provider.hpp"
#include "bootstrap/FlightTransitions.hpp"

namespace batbox::application_commands {
struct McpAuthUi {
  std::string server,url,callbackText;
  std::function<void(const std::string&)> submitCallback;
  std::function<void()> changed;
};
struct Inputs {
  event_loop::Loop& owner;
  application::Config config;
  std::shared_ptr<app_state::AppStateStore> store;
  std::shared_ptr<application_compaction_settings::Policy> compactionPolicy;
  interactive::PresentationState& view;
  std::shared_ptr<application_model::LocalModelService> model;
  std::shared_ptr<application_poetry::Provider> poetry;
  std::shared_ptr<application_flights::Flight> flight;
  std::shared_ptr<application_engine::Compactor> compactor;
  std::shared_ptr<mcp::manager::IndigenousRuntime> mcpRuntime;
  std::shared_ptr<McpAuthUi> mcpAuthUi;
  std::shared_ptr<bootstrap::InvokedSkillsState> invokedSkills;
  std::shared_ptr<application_skills::Provider> skills;
  std::shared_ptr<application_memory::Provider> memory;
  mcp::ResultServices mcpPoetryResults;
  std::function<event_loop::Deferred<Json>(const std::string&)>
    mcpPoetryAttachments;
  std::function<std::shared_ptr<bats::BatContext>()> getBatContext;
  std::shared_ptr<application_hooks::Provider> hooks;
  std::shared_ptr<application_permissions::PermissionProvider> permission;
  std::shared_ptr<bootstrap::FlightTransitions> flightTransitions;
  std::shared_ptr<messages::Services> messages;
  std::shared_ptr<file_state_stash::FileStateStash> fileStash;
  application_shell::Host managedShell;
  std::shared_ptr<runtime::OrderedStringSet> discoveredSkills,loadedMemoryPaths;
  std::function<std::string(const std::string&)> batTranscriptPath;
  std::function<void(const std::string&,const Json&)> killForegroundMission;
  std::function<void(const std::string&)> stopBatMission;
  std::function<bool(const std::string&)> backgroundBatMission;
  std::function<void(const std::string&)> openBatConversation,dismissBatMission;
  std::function<Json()> batDefinitions;
  std::function<void()> reloadBatDefinitions;
  std::function<std::shared_ptr<application_runtime::ApplicationRuntime>()> getRuntime;
  std::function<std::shared_ptr<interactive::conversation_shell::Shell>()> getShell;
  std::function<std::shared_ptr<interactive::presentation_providers::ThemeState>()> theme;
  std::function<void()> openRewind;
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
} // namespace batbox::application_commands
