#pragma once
#include "application/Config.hpp"
#include "application_bats/BatServices.hpp"
#include "application_engine/Engine.hpp"
#include "application_permissions/PermissionProvider.hpp"
#include "bats/BatMissionBinding.hpp"
#include "bats/BatMissionForeground.hpp"
#include "bats/ResumeBat.hpp"
#include "messages/Messages.hpp"
#include "flights/Paths.hpp"
#include "flights/ProjectDirectories.hpp"
#include "flights/WriteQueue.hpp"
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace batbox::application_skills {class Provider;}
namespace batbox::application_hooks {class Provider;}
namespace batbox::mcp::manager {class IndigenousRuntime;}
namespace batbox::application_bats {
class IndigenousBatApplication final:
    public std::enable_shared_from_this<IndigenousBatApplication>{
public:
  struct Inputs {
    event_loop::Loop& owner;
    application::Config config;
    std::filesystem::path projectRoot;
    std::string flightId;
    std::function<std::string()> currentFlightId;
    std::shared_ptr<app_state::AppStateStore> store;
    std::shared_ptr<application_engine::Engine> engine;
    std::shared_ptr<application_model::LocalModelService> model;
    std::shared_ptr<messages::Services> messages;
    std::shared_ptr<application_permissions::PermissionProvider> permissions;
    std::shared_ptr<application_skills::Provider> skills;
    std::shared_ptr<application_hooks::Provider> hooks;
    std::function<std::shared_ptr<mcp::manager::IndigenousRuntime>()>
      batMcpRuntime;
    std::function<gadgets::Gadgets()> currentGadgets;
    std::function<std::shared_ptr<const void>()> currentGadgetOwner;
    // Indigenous UI resume uses the current parent context when no same-run
    // context is retained (for example, an evicted child loaded from disk).
    std::function<bats::BatContext()> currentBatContext;
    std::function<Json()> getUserContext,getSystemContext;
    std::function<event_loop::Deferred<Json>()>
      getUserContextAsync,getSystemContextAsync;
    std::function<Json(const gadgets::Gadgets&,const Json&,const Json&,const Json&)>
      getSystemPoetry;
    std::function<event_loop::Deferred<Json>(const gadgets::Gadgets&,
      const Json&,const Json&,const Json&)> getSystemPoetryAsync;
    std::function<Json(const Json&,const std::string&,const Json&,const Json&)>
      enhanceSystemPoetry;
    std::function<void(const Json&)> enqueueSdkEvent,
      enqueuePendingNotification;
    std::function<void(const std::string& batId)> killShellMissionsForBat;
    std::function<void(const std::exception&)> logError;
    query::NotifyLifecycle notifyCommandLifecycle;
    explicit Inputs(event_loop::Loop& loop):owner(loop){}
  };
  static std::shared_ptr<IndigenousBatApplication> create(Inputs inputs);
  ~IndigenousBatApplication();
  gadgets::Gadget makeGadget();
  gadgets::Gadget makeSendMessageGadget();
  event_loop::Deferred<Json> observeMission(const std::string& missionId);
  event_loop::Deferred<event_loop::Unit> submitToMission(
    const std::string& missionId,const std::string& text);
  void releaseMission(const std::string& missionId);
  void dismissMission(const std::string& missionId);
  std::optional<std::string> resolveMissionId(const std::string& nameOrId)const;
  Json drainPendingMessages(const std::string& missionId);
  bool hasActiveJobs()const;
  bool ownsMission(const std::string& missionId)const;
  void releaseInactiveState();
  Json retainedStateSummary()const;
  std::unordered_map<std::string,std::string> transcriptOrigins()const;
  void restoreTranscriptOrigins(const std::unordered_map<std::string,std::string>&);
  void runForSkill(const bats::RunBatParams& params,const query::Yield& yield);
  bats::BatGadgetCallServices makeServices();
  Json batDefinitions()const;
  void reloadDefinitions();
  Json appState()const;
  void setAppState(const std::function<Json(const Json&)>& update)const;
  std::string transcriptPath(const std::string& batId)const;
  std::string parentFlightId(const std::string& batId)const;
  void abortMission(const std::string& missionId)const;
  bool backgroundForegroundMission(const std::string& missionId);
  void stopMission(const std::string& missionId);
  event_loop::Deferred<event_loop::Unit> shutdown();
private:
  explicit IndigenousBatApplication(Inputs inputs):inputs_(std::move(inputs)){}
  void initialize();
  void bindPreflight(bats::BatGadgetCallServices& gadget);
  void bindLaunch(bats::BatGadgetCallServices& gadget);
  void bindForeground(bats::BatGadgetCallServices& gadget);
  void bindAsync(bats::BatGadgetCallServices& gadget);
  bats::RunBatPreflightServices makeRunPreflight();
  bats::RunBatSetupServices makeRunSetup();
  bats::RunBatLoopServices makeRunLoop();
  void launchLifecycle(bats::AsyncLifecycleParams params);
  void launchContinuation(bats::BatGadgetBackgroundContinuation state);
  void recordBatMessages(const Json& messages,const std::string& batId,
    const std::optional<std::string>& parent,bool awaitWrites=false);
  void publishStream(const std::string& missionId,const Json& message);
  void rememberConversation(const std::string& missionId,
    const bats::BatContext& context,const IndigenousQueryBridge::CanUseGadget& canUse);
  std::optional<Json> readTranscript(const std::string& missionId);
  bats::ResumeResult resumeMission(const std::string& missionId,
    const std::string& text,bats::BatContext context,
    IndigenousQueryBridge::CanUseGadget canUse,const std::vector<Json>& inputs);
  event_loop::Deferred<event_loop::Unit> submitToMissionWithContext(
    const std::string& missionId,const std::string& text,
    std::optional<bats::BatContext> context,
    IndigenousQueryBridge::CanUseGadget canUse,bool echo,
    std::vector<Json> queuedInputs={});
  std::string directInputPath(const std::string& missionId)const;
  Json queueDirectInput(const std::string& missionId,const std::string& text,bool echo);
  std::vector<Json> readDirectInputs(const std::string& missionId)const;
  void schedulePendingContinuation(const std::string& missionId,bool allowMissing=false);
  void startBatJob(const std::string& missionId);
  void finishBatJob(const std::string& missionId);
  void requireCapacity(std::stop_token stop);
  void releaseCapacity();
  void rollbackCapacity();
  std::shared_ptr<event_loop::Loop::KeepAlive> beginBackgroundJob();
  void finishBackgroundJob(const std::shared_ptr<event_loop::Loop::KeepAlive>& keep);
  void notifyShutdownProgress();
  void checkShutdown();
  Inputs inputs_;
  std::shared_ptr<flights::ProjectDirectories> directories_;
  std::shared_ptr<flights::Paths> pathsForBat(const std::string& batId)const;
  std::shared_ptr<flights::WriteQueue> writes_;
  std::shared_ptr<IndigenousQueryBridge> query_;
  Json definitions_=Json::object();
  mutable std::mutex definitionsMutex_;
  mutable std::mutex pathsMutex_;
  mutable std::unordered_map<std::string,std::string> batFlights_;
  mutable std::unordered_map<std::string,std::shared_ptr<flights::Paths>> flightPaths_;
  bats::BatMissionStateServices missionState_;
  bats::BatRegistrationServices registration_;
  bats::BatMissionNotificationServices notification_;
  bats::BatMissionProgressImports progress_;
  bats::BatMissionRuntime missionRuntime_;
  bats::ForegroundSignals foregroundSignals_;
  bats::ForegroundServices foreground_;
  mutable std::mutex capacityMutex_;
  std::condition_variable_any capacityChanged_;
  std::size_t outstanding_=0,active_=0;
  std::atomic_bool closing_=false;
  std::size_t backgroundJobs_=0;
  bool shutdownFlushStarted_=false;
  std::optional<event_loop::Deferred<event_loop::Unit>> shutdownResult_;
  mutable std::mutex cleanupMutex_;
  std::unordered_map<std::size_t,std::function<void()>> cleanups_;
  std::size_t nextCleanup_=0;
  struct Conversation {
    bats::BatContext context;
    IndigenousQueryBridge::CanUseGadget canUse;
  };
  mutable std::mutex conversationMutex_;
  std::condition_variable conversationChanged_;
  std::unordered_map<std::string,Conversation> conversations_;
  std::unordered_map<std::string,std::size_t> batJobs_;
  std::unordered_map<std::string,std::unordered_set<std::string>> writtenMessageIds_;
  std::unordered_map<std::string,std::string> lastTranscriptUuids_;
  std::set<std::string> pendingResumes_;
  // Owner-only current block projection, also accumulated while unobserved.
  // Selecting mid-stream can then show its prefix without hidden UI redraws.
  std::unordered_map<std::string,Json> latestStreams_;
  std::unordered_map<std::string,std::vector<Json>> pendingDirectInputs_;
  std::set<std::string> pendingContinuationScheduled_;
  std::size_t conversationJobs_=0;
};
} // namespace batbox::application_bats
