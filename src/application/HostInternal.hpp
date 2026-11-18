#pragma once
#include "application/Host.hpp"
#include "application/HostCron.hpp"
#include "application/HostShellSnapshot.hpp"
#include "application/HostRetention.hpp"
#include "diagnostics/DebugLog.hpp"
#include "application/bats/IndigenousBatApplication.hpp"
#include "application/connectors/Bootstrap.hpp"
#include "application/engine/Compactor.hpp"
#include "application/engine/Engine.hpp"
#include "application/hooks/Provider.hpp"
#include "application/hooks/FileChangedWatcher.hpp"
#include "application/permissions/PermissionProvider.hpp"
#include "application/presentation/Bindings.hpp"
#include "application/poetry/Provider.hpp"
#include "application/runtime/ApplicationRuntime.hpp"
#include "application/flights/Flight.hpp"
#include "application/flights/Restore.hpp"
#include "application/shell/ApplicationShell.hpp"
#include "application/frontend/Defaults.hpp"
#include "application/commands/Commands.hpp"
#include "application/gadgets/ApplicationGadgets.hpp"
#include "application/roost/Roost.hpp"
#include "application/media/Bindings.hpp"
#include "application/media/AttachmentAggregate.hpp"
#include "application/history/History.hpp"
#include "application/completion/Completion.hpp"
#include "application/cli/Adapter.hpp"
#include "application/file_history/History.hpp"
#include "application/skills/Provider.hpp"
#include "application/memory/Provider.hpp"
#include "application/lsp/Runtime.hpp"
#include "application/mission_ui/BatMissionBridge.hpp"
#include "mcp/manager/IndigenousRuntime.hpp"
#include "application/mcp_elicitation/Controller.hpp"
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
    std::unordered_map<std::string,application::bats::BatNode> batLineage;
    enum class State {Live, Closing, Dormant, Reviving};
    State state=State::Live;
    std::size_t pendingOperations=0;
    std::shared_ptr<application::poetry::Provider> poetry;
    std::shared_ptr<application::skills::Provider> skills;
    std::shared_ptr<application::file_history::History> fileHistory;
    std::shared_ptr<application::mission_ui::BatMissionBridge> missionBridge;
    application::shell::Host managedShell;
    gadget_results::StorageImports storage;
    std::shared_ptr<application::hooks::Provider> hooks;
    std::shared_ptr<application::engine::Compactor> compactor;
    std::shared_ptr<application::engine::Engine> engine;
    std::shared_ptr<application::bats::IndigenousBatApplication> bats;
    std::shared_ptr<application::commands::Bundle> commands;
    std::shared_ptr<application::media::AggregateProvider> attachments;
    std::shared_ptr<mcp::manager::IndigenousRuntime> mcpRuntime;
    std::shared_ptr<batbox::gadgets::live_roster::Roster> roster;
    std::shared_ptr<const batbox::gadgets::live_roster::Snapshot> gadgets;
    std::shared_ptr<application::memory::Provider> memory;
    std::shared_ptr<application::lsp::Runtime> lsp;
    std::shared_ptr<std::weak_ptr<application::engine::Engine>> generationEngineSlot;
    std::shared_ptr<std::weak_ptr<application::bats::IndigenousBatApplication>> generationBatSlot;
    std::shared_ptr<std::weak_ptr<mcp::manager::IndigenousRuntime>> mcpRuntimeSlot;
    std::shared_ptr<Json> mcpClientRows,commandRows;
    std::shared_ptr<application::commands::McpAuthUi> mcpAuthUi;
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
  std::shared_ptr<application::compaction_settings::Policy> compactionPolicy;
  std::shared_ptr<application::mcp_elicitation::Controller> mcpElicitation;
  std::shared_ptr<application::model::LocalModelService> model;
  std::shared_ptr<application::poetry::Provider> poetry;
  std::shared_ptr<application::skills::Provider> skills;
  std::shared_ptr<application::memory::Provider> memory;
  std::shared_ptr<application::lsp::Runtime> lsp;
  std::shared_ptr<std::weak_ptr<application::engine::Engine>> generationEngineSlot;
  std::shared_ptr<std::weak_ptr<application::bats::IndigenousBatApplication>> generationBatSlot;
  std::shared_ptr<application::file_history::History> fileHistory;
  std::shared_ptr<application::mission_ui::BatMissionBridge> missionBridge;
  std::shared_ptr<application::connectors::Bundle> connectors;
  std::shared_ptr<mcp::manager::IndigenousRuntime> mcpRuntime;
  std::shared_ptr<std::weak_ptr<mcp::manager::IndigenousRuntime>> mcpRuntimeSlot;
  std::shared_ptr<Json> mcpClientRows;
  std::shared_ptr<Json> commandRows;
  std::shared_ptr<application::commands::McpAuthUi> mcpAuthUi;
  std::shared_ptr<application::permissions::PermissionProvider> permission;
  std::shared_ptr<application::flights::Flight> flight;
  std::shared_ptr<application::roost::Roost> roost;
  std::shared_ptr<application::media::Bundle> media;
  std::shared_ptr<application::media::AggregateProvider> attachments;
  std::shared_ptr<application::history::Bundle> history;
  std::shared_ptr<application::completion::Provider> completion;
  std::shared_ptr<application::cli::Adapter> cli;
  std::shared_ptr<CronWiring> cron;
  std::shared_ptr<ExitSteps> exitSteps;
  event_loop::Loop::TimerHandle retentionTimer;
  std::function<std::shared_future<std::optional<std::string>>(const std::string&)>
    shellSnapshot;
  std::shared_ptr<Json> sdkThinkingConfig;
  std::shared_ptr<bool> sdkThinkingOverride;
  std::shared_ptr<bootstrap::FlightTransitions> flightTransitions;
  std::shared_ptr<application::flights::Restore> restore;
  std::shared_ptr<application::hooks::Provider> hooks;
  std::shared_ptr<application::hooks::FileChangedWatcher> fileWatcher;
  std::shared_ptr<application::engine::Compactor> compactor;
  std::shared_ptr<bootstrap::InvokedSkillsState> invokedSkills;
  std::shared_ptr<application::engine::Engine> engine;
  std::shared_ptr<application::bats::IndigenousBatApplication> bats;
  std::shared_ptr<application::runtime::ApplicationRuntime> runtime;
  std::shared_ptr<application::frontend::Bundle> frontend;
  std::shared_ptr<application::commands::Bundle> commands;
  std::shared_ptr<application::presentation::Bundle> presentation;
  std::shared_ptr<interactive::conversation_shell::Shell> shell;
  std::shared_ptr<interactive::presentation_providers::ThemeState> theme;
  std::shared_ptr<interactive::presentation_providers::SystemThemeStash> systemTheme;
  std::shared_ptr<batbox::gadgets::live_roster::Roster> roster;
  std::optional<application::gadgets::Prepared> preparedGadgets;
  mcp::Services mcpServices;
  std::vector<RetiredRoost> retiredRoosts;
  std::optional<std::filesystem::path> bindingProjectRootOverride;
  bool bindingRetired=false;
  std::optional<event_loop::Loop::TimerHandle> retirementPoll;
  application::shell::Host managedShell;
  gadget_results::StorageImports storage;
  std::shared_ptr<file_state_stash::FileStateStash> fileStash;
  std::shared_ptr<gadget_results::ContentReplacementState> replacements;
  std::shared_ptr<batbox::runtime::OrderedStringSet> loadedMemoryPaths,discoveredSkills;
  std::shared_ptr<batbox::bats::BatContext> currentBat;
  std::shared_ptr<batbox::runtime::OrderedStringSet> inProgressGadgetUseIds;
  std::string statusText;
  std::optional<std::pair<std::string,std::string>> editorNotification;
  std::shared_ptr<std::ofstream> logFile;
  // Present only when the command line asked for a debug log.
  std::shared_ptr<diagnostics::DebugLog> debugLog;
  std::shared_ptr<std::mutex> logMutex;
  // Guarded by logMutex: a flush is already scheduled on the owner loop.
  std::shared_ptr<bool> logFlushPending;
  std::function<void()> scheduleLogFlush;
  bool closing=false;
  std::atomic_bool failed=false;
  // Set on the owner loop while the user has poetry queued for the main
  // Flight; Gadget threads read it to stop waiting so it is delivered.
  std::shared_ptr<std::atomic_bool> userMessageQueued=std::make_shared<std::atomic_bool>(false);
  event_loop::Loop::TimerHandle userMessagePoll;
  bool userMessagePolling=false;
  void initialize();
  void backgroundForUserMessage();
  void bindBackend();
  void bindMcpRuntime();
  void refreshCommandCatalog(const std::shared_ptr<batbox::gadgets::live_roster::Roster>& generation);
  void bindFrontend();
  void bindRoostProviders();
  void bindStorage();
  void bindFileHistory();
  std::shared_ptr<application::commands::Bundle> makeCommands();
  interactive::context::ContextBindings makeContextBindings();
  void rebindRoost(const application::roost::Snapshot& previous,
    const application::roost::Snapshot& next);
  void releaseRetiredProviders();
  RetiredRoost captureRoostGeneration()const;
  void installRoostGeneration(const RetiredRoost& generation);
  std::filesystem::path bindingProjectRoot()const;
  std::shared_ptr<application::bats::IndigenousBatApplication> batOwner(
    const std::string& id);
  std::optional<std::size_t> retiredIndexForBat(const std::string& id)const;
  void finishRetiredBatOperation(std::optional<std::size_t> index);
  event_loop::Deferred<event_loop::Unit> batOwnerReady(const std::string& id);
  event_loop::Deferred<Json> observeBatMission(const std::string& id);
  event_loop::Deferred<event_loop::Unit> submitBatMessage(
    const std::string& id,const std::string& text);
  std::shared_ptr<application::bats::IndigenousBatApplication> reviveRetired(
    std::size_t index);
  void beginRetiredShutdown(std::size_t index);
  Json runtimeMemorySummary()const;
  void shutdown();
  void requestExit();
  void log(std::string text)const;
  void flushLog()const;
  void report(std::exception_ptr error,std::string_view origin="the application");
  batbox::gadgets::Gadgets currentGadgets()const;
  Json appState()const;
  // After the terminal is restored: how to come back to this Flight.
  void printResumeHint();
};
}
