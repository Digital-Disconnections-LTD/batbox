#pragma once

#include "bats/BatGadgetCall.hpp"
#include "application/Config.hpp"
#include "interactive/TurnBinding.hpp"
#include "interactive/context/ContextFactory.hpp"
#include "interactive/lifecycle/LoadingState.hpp"
#include "interactive/repl_runtime/Runtime.hpp"
#include <memory>
#include <optional>

namespace batbox::interactive::conversation_shell {class Shell;struct Imports;}
namespace batbox::application_runtime {

// Transcript persistence services. The flight provider remains
// dynamic so /clear and /resume can switch transcript identity before the
// next stable message-array publication.
struct TranscriptPersistence {
  std::function<std::string()> flightId;
  std::function<event_loop::Deferred<std::optional<std::string>>(
    const std::vector<interactive::MessageRef>&,
    std::optional<std::string>,
    const std::vector<interactive::MessageRef>&)> record;
  std::function<std::optional<std::string>(
    const std::vector<interactive::MessageRef>&,
    const std::vector<interactive::MessageRef>&)> lastChainParticipant;
};

// Backend, poetry, gadget and process providers are supplied by the executable.
// The owner below binds the stateful edges shared by the interactive path.
struct Inputs {
  event_loop::Loop& owner;
  application::Config config;
  std::shared_ptr<app_state::AppStateStore> store;
  interactive::PresentationState& presentation;
  interactive::TurnMessagesRef initialMessages;
  std::shared_ptr<file_state_stash::FileStateStash> readFileState;
  std::shared_ptr<query::AsyncEngineServices> engine;
  query::NotifyLifecycle notifyLifecycle;
  std::vector<user_input::CommandEntry> commands;
  interactive::context::ContextBindings context;
  interactive::QueryTurnServices turn;
  interactive::QueryExecutionServices execution;
  interactive::lifecycle::LoadingServices loading;
  interactive::repl_runtime::TranscriptServices transcript;
  interactive::repl_runtime::IdleServices idle;
  interactive::PresentationImports presentationImports;
  interactive::QueryExecutionParamsFactory makeQueryParams;
  std::function<void(const interactive::InteractiveQueryContext&)>
    onQueryContextCreated;
  std::function<void(const interactive::TurnRequest&)> onTurnRequested;
  std::function<interactive::TurnMessageRef()> createInterruptionMessage;
  std::function<void(interactive::submit::SubmitServices&)> bindSubmitProviders;
  std::function<void(interactive::repl_runtime::RuntimeServices&)> bindRuntimeProviders;
  std::function<interactive::submit::PoetrySubmitRequest(
    const std::string&,const interactive::InputState&)> makeSubmitRequest;
  std::function<bats::BatGadgetCallServices()> makeBatGadgetServices;
  std::function<void()> requestRedraw;
  std::function<double()> nowMs;
  std::function<bool(const std::string&)> feature;
  std::function<void(std::exception_ptr)> onDetachedError;
  TranscriptPersistence transcriptPersistence;
  explicit Inputs(event_loop::Loop& loop,interactive::PresentationState& view):
    owner(loop),presentation(view){}
};
// Indigenous owner replacement between turns. All fields describe one roost
// generation; the caller keeps background mission owners from older generations
// alive until their missions settle.
struct EnvironmentRebind {
  application::Config config;
  std::shared_ptr<query::AsyncEngineServices> engine;
  query::NotifyLifecycle notifyLifecycle;
  std::vector<user_input::CommandEntry> commands;
  interactive::context::ContextBindings context;
  std::function<void()> onCommandsInstalled;
};

class ApplicationRuntime final:public std::enable_shared_from_this<ApplicationRuntime>{
public:
  static std::shared_ptr<ApplicationRuntime> create(Inputs inputs);
  ~ApplicationRuntime();
  void start();
  void stop();
  event_loop::Deferred<event_loop::Unit> stopAndWait();
  event_loop::Deferred<event_loop::Unit> awaitTranscriptIdle();
  // Install after the Shell exists; its callback applies the stream event on
  // the same owner thread and may request a frame. The owner still publishes
  // changed message references to MessageState after that callback returns.
  void setStreamSink(std::function<void(interactive::StreamInput)> sink);
  void setModalSink(std::function<void(std::optional<interactive::UiRequest>)> sink);
  void setRedrawSink(std::function<void()> sink);
  void attachShell(std::shared_ptr<interactive::conversation_shell::Shell> shell);
  void bindShellImports(interactive::conversation_shell::Imports& imports);
  std::shared_ptr<interactive::repl_runtime::Runtime> runtime()const{return runtime_;}
  std::shared_ptr<interactive::submit::SubmitServices> submitServices()const{return submit_;}
  std::shared_ptr<interactive::QueryExecutionServices> executionServices()const{
    return execution_;
  }
  interactive::submit::CommandQueue& queue()const{return *queue_;}
  interactive::lifecycle::MessageState& messages()const{return *messages_;}
  interactive::lifecycle::LoadingState& loading()const{return *loading_;}
  std::shared_ptr<interactive::QueryTurnState> turnState()const{return turnState_;}
  std::shared_ptr<interactive::TurnAbort> activeAbort()const{return activeAbort_;}
  // Local commands reserve the guard while their UI runs. Conversation
  // replacement is blocked only after a model turn actually starts.
  bool canReplaceConversation()const{
    return !turnState_||!turnState_->guard.isRunning();
  }
  void rebindEnvironment(EnvironmentRebind next);
  void replaceCommands(std::vector<user_input::CommandEntry> commands,
    std::function<void()> onInstalled={});
  std::shared_ptr<interactive::context::ContextFactory> contextFactory()const{return context_;}
  const std::function<bats::BatGadgetCallServices()>& batGadgetServices()const{
    return inputs_.makeBatGadgetServices;
  }
  interactive::submit::PoetrySubmitRequest poetryRequest(
    const std::string& text,const interactive::InputState& input)const;
private:
  explicit ApplicationRuntime(Inputs inputs):inputs_(std::move(inputs)){}
  void initialize();
  void bindContext();
  std::shared_ptr<interactive::context::ContextFactory> makeContextFactory(
    interactive::context::ContextBindings bindings);
  void installCommands(std::vector<user_input::CommandEntry> commands,
    std::function<void()> onInstalled);
  void bindQuery();
  void bindSubmit();
  void bindRuntime();
  void acceptStep(const query::AsyncEngineStep& step);
  void publishMessageChanges(std::size_t beforeSize,const interactive::MessageRef& beforeLast);
  void recordTranscriptChanges(const interactive::TurnMessagesRef& messages);
  void requestRedraw()const;
  Inputs inputs_;
  std::unique_ptr<interactive::submit::CommandQueue> queue_;
  std::unique_ptr<interactive::lifecycle::MessageState> messages_;
  std::shared_ptr<interactive::lifecycle::LoadingState> loading_;
  std::shared_ptr<interactive::context::ContextFactory> context_;
  std::shared_ptr<interactive::QueryTurnState> turnState_;
  std::shared_ptr<interactive::QueryExecutionState> executionState_;
  std::shared_ptr<interactive::QueryTurnServices> turn_;
  std::shared_ptr<interactive::QueryExecutionServices> execution_;
  std::shared_ptr<interactive::submit::SubmitServices> submit_;
  std::shared_ptr<interactive::repl_runtime::Runtime> runtime_;
  std::shared_ptr<interactive::TurnAbort> activeAbort_;
  std::weak_ptr<interactive::TurnAbort> interruptedTurn_;
  std::function<void(interactive::StreamInput)> streamSink_;
  std::function<void(std::optional<interactive::UiRequest>)> modalSink_;
  std::function<void()> redrawSink_;
  std::optional<std::vector<user_input::CommandEntry>> pendingCommands_;
  std::function<void()> pendingCommandsInstalled_;
  std::function<void()> unsubscribeGuard_;
  std::vector<event_loop::Deferred<event_loop::Unit>> activeTurns_;
  std::vector<event_loop::Deferred<std::optional<std::string>>>
    activeTranscriptRecords_;
  std::vector<event_loop::Deferred<event_loop::Unit>> transcriptIdleWaiters_;
  std::optional<event_loop::Deferred<event_loop::Unit>> stopWait_;
  std::size_t lastRecordedLength_=0;
  std::size_t transcriptCallSeq_=0;
  std::optional<std::string> firstRecordedUuid_,lastRecordedParent_,
    transcriptFlightId_;
  bool stopping_=false;
  bool started_=false;
};
} // namespace batbox::application_runtime
