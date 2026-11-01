#pragma once
#include "application/Host.hpp"
#include "application_bats/IndigenousBatApplication.hpp"
#include "application_connectors/Bootstrap.hpp"
#include "application_engine/Compactor.hpp"
#include "application_engine/Engine.hpp"
#include "application_hooks/Provider.hpp"
#include "application_hooks/FileChangedWatcher.hpp"
#include "application_permissions/PermissionProvider.hpp"
#include "application_presentation/Bindings.hpp"
#include "application_poetry/Provider.hpp"
#include "application_runtime/ApplicationRuntime.hpp"
#include "application_flights/Flight.hpp"
#include "application_flights/Restore.hpp"
#include "application_shell/ApplicationShell.hpp"
#include "application_frontend/Defaults.hpp"
#include "application_commands/Commands.hpp"
#include "application_gadgets/ApplicationGadgets.hpp"
#include "application_roost/Roost.hpp"
#include "application_media/Bindings.hpp"
#include "application_media/AttachmentAggregate.hpp"
#include "application_history/History.hpp"
#include "application_completion/Completion.hpp"
#include "application_cli/Adapter.hpp"
#include "application_file_history/History.hpp"
#include "application_skills/Provider.hpp"
#include "application_memory/Provider.hpp"
#include "application_lsp/Runtime.hpp"
#include "application_mission_ui/BatMissionBridge.hpp"
#include "mcp/manager/IndigenousRuntime.hpp"
#include "application_mcp_elicitation/Controller.hpp"
#include "event_loop/Loop.hpp"
#include "gadgets/live_roster/LiveRoster.hpp"
#include <atomic>
#include <fstream>
#include <mutex>
#include <unordered_map>

namespace batbox::application {
struct Host::Impl {
  explicit Impl(Config selected):config(std::move(selected)){}
  struct RetiredRoost {
    Config config;
    std::filesystem::path projectRoot;
    Json settings=Json::object(),mcpSnapshot=Json::object();
    std::unordered_map<std::string,std::string> transcriptOrigins;
    enum class State {Live, Closing, Dormant, Reviving};
    State state=State::Live;
    std::size_t pendingOperations=0;
    std::shared_ptr<application_poetry::Provider> poetry;
    std::shared_ptr<application_skills::Provider> skills;
    std::shared_ptr<application_file_history::History> fileHistory;
    std::shared_ptr<application_mission_ui::BatMissionBridge> missionBridge;
    application_shell::Host managedShell;
    gadget_results::StorageImports storage;
    std::shared_ptr<application_hooks::Provider> hooks;
    std::shared_ptr<application_engine::Compactor> compactor;
    std::shared_ptr<application_engine::Engine> engine;
    std::shared_ptr<application_bats::IndigenousBatApplication> bats;
    std::shared_ptr<application_commands::Bundle> commands;
    std::shared_ptr<application_media::AggregateProvider> attachments;
    std::shared_ptr<mcp::manager::IndigenousRuntime> mcpRuntime;
    std::shared_ptr<gadgets::live_roster::Roster> roster;
    std::shared_ptr<const gadgets::live_roster::Snapshot> gadgets;
    std::shared_ptr<application_memory::Provider> memory;
    std::shared_ptr<application_lsp::Runtime> lsp;
    std::shared_ptr<std::weak_ptr<application_engine::Engine>> generationEngineSlot;
    std::shared_ptr<std::weak_ptr<application_bats::IndigenousBatApplication>> generationBatSlot;
    std::shared_ptr<std::weak_ptr<mcp::manager::IndigenousRuntime>> mcpRuntimeSlot;
    std::shared_ptr<Json> mcpClientRows,commandRows;
    std::shared_ptr<application_commands::McpAuthUi> mcpAuthUi;
    mcp::Services mcpServices;
    std::optional<event_loop::Deferred<event_loop::Unit>> mcpReady;
    std::optional<event_loop::Deferred<event_loop::Unit>> shutdownReady;
  };
  Config config;
  event_loop::Loop owner;
  std::string flightId;
  std::optional<std::string> hookFlightOverride;
  interactive::PresentationState view;
  std::shared_ptr<messages::Services> messages;
  std::shared_ptr<app_state::AppStateStore> store;
  std::shared_ptr<application_compaction_settings::Policy> compactionPolicy;
  std::shared_ptr<application_mcp_elicitation::Controller> mcpElicitation;
  std::shared_ptr<application_model::LocalModelService> model;
  std::shared_ptr<application_poetry::Provider> poetry;
  std::shared_ptr<application_skills::Provider> skills;
  std::shared_ptr<application_memory::Provider> memory;
  std::shared_ptr<application_lsp::Runtime> lsp;
  std::shared_ptr<std::weak_ptr<application_engine::Engine>> generationEngineSlot;
  std::shared_ptr<std::weak_ptr<application_bats::IndigenousBatApplication>> generationBatSlot;
  std::shared_ptr<application_file_history::History> fileHistory;
  std::shared_ptr<application_mission_ui::BatMissionBridge> missionBridge;
  std::shared_ptr<application_connectors::Bundle> connectors;
  std::shared_ptr<mcp::manager::IndigenousRuntime> mcpRuntime;
  std::shared_ptr<std::weak_ptr<mcp::manager::IndigenousRuntime>> mcpRuntimeSlot;
  std::shared_ptr<Json> mcpClientRows;
  std::shared_ptr<Json> commandRows;
  std::shared_ptr<application_commands::McpAuthUi> mcpAuthUi;
  std::shared_ptr<application_permissions::PermissionProvider> permission;
  std::shared_ptr<application_flights::Flight> flight;
  std::shared_ptr<application_roost::Roost> roost;
  std::shared_ptr<application_media::Bundle> media;
  std::shared_ptr<application_media::AggregateProvider> attachments;
  std::shared_ptr<application_history::Bundle> history;
  std::shared_ptr<application_completion::Provider> completion;
  std::shared_ptr<application_cli::Adapter> cli;
  std::shared_ptr<Json> sdkThinkingConfig;
  std::shared_ptr<bool> sdkThinkingOverride;
  std::shared_ptr<bootstrap::FlightTransitions> flightTransitions;
  std::shared_ptr<application_flights::Restore> restore;
  std::shared_ptr<application_hooks::Provider> hooks;
  std::shared_ptr<application_hooks::FileChangedWatcher> fileWatcher;
  std::shared_ptr<application_engine::Compactor> compactor;
  std::shared_ptr<bootstrap::InvokedSkillsState> invokedSkills;
  std::shared_ptr<application_engine::Engine> engine;
  std::shared_ptr<application_bats::IndigenousBatApplication> bats;
  std::shared_ptr<application_runtime::ApplicationRuntime> runtime;
  std::shared_ptr<application_frontend::Bundle> frontend;
  std::shared_ptr<application_commands::Bundle> commands;
  std::shared_ptr<application_presentation::Bundle> presentation;
  std::shared_ptr<interactive::conversation_shell::Shell> shell;
  std::shared_ptr<interactive::presentation_providers::ThemeState> theme;
  std::shared_ptr<interactive::presentation_providers::SystemThemeStash> systemTheme;
  std::shared_ptr<gadgets::live_roster::Roster> roster;
  std::optional<application_gadgets::Prepared> preparedGadgets;
  mcp::Services mcpServices;
  std::vector<RetiredRoost> retiredRoosts;
  std::optional<std::filesystem::path> bindingProjectRootOverride;
  bool bindingRetired=false;
  std::optional<event_loop::Loop::TimerHandle> retirementPoll;
  application_shell::Host managedShell;
  gadget_results::StorageImports storage;
  std::shared_ptr<file_state_stash::FileStateStash> fileStash;
  std::shared_ptr<gadget_results::ContentReplacementState> replacements;
  std::shared_ptr<runtime::OrderedStringSet> loadedMemoryPaths,discoveredSkills;
  std::shared_ptr<bats::BatContext> currentBat;
  std::shared_ptr<runtime::OrderedStringSet> inProgressGadgetUseIds;
  std::string statusText;
  std::optional<std::pair<std::string,std::string>> editorNotification;
  std::shared_ptr<std::ofstream> logFile;
  std::shared_ptr<std::mutex> logMutex;
  bool closing=false;
  std::atomic_bool failed=false;
  void initialize();
  void bindBackend();
  void bindMcpRuntime();
  void refreshCommandCatalog(const std::shared_ptr<gadgets::live_roster::Roster>& generation);
  void bindFrontend();
  void bindRoostProviders();
  void bindStorage();
  void bindFileHistory();
  std::shared_ptr<application_commands::Bundle> makeCommands();
  interactive::context::ContextBindings makeContextBindings();
  void rebindRoost(const application_roost::Snapshot& previous,
    const application_roost::Snapshot& next);
  void releaseRetiredProviders();
  RetiredRoost captureRoostGeneration()const;
  void installRoostGeneration(const RetiredRoost& generation);
  std::filesystem::path bindingProjectRoot()const;
  std::shared_ptr<application_bats::IndigenousBatApplication> batOwner(
    const std::string& id);
  std::optional<std::size_t> retiredIndexForBat(const std::string& id)const;
  void finishRetiredBatOperation(std::optional<std::size_t> index);
  event_loop::Deferred<event_loop::Unit> batOwnerReady(const std::string& id);
  event_loop::Deferred<Json> observeBatMission(const std::string& id);
  event_loop::Deferred<event_loop::Unit> submitBatMessage(
    const std::string& id,const std::string& text);
  std::shared_ptr<application_bats::IndigenousBatApplication> reviveRetired(
    std::size_t index);
  void beginRetiredShutdown(std::size_t index);
  Json runtimeMemorySummary()const;
  void shutdown();
  void requestExit();
  void log(std::string text)const;
  void report(std::exception_ptr error);
  gadgets::Gadgets currentGadgets()const;
  Json appState()const;
};
}
