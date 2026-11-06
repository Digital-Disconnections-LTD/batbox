#pragma once
#include "file_state_stash/FileStateStash.hpp"
#include "interactive/PresentationState.hpp"
#include "interactive/QueryTurn.hpp"
#include "interactive/lifecycle/MessageState.hpp"
#include "interactive/submit/PoetrySubmit.hpp"
#include "interactive/submit/QueueReadiness.hpp"

namespace batbox::interactive::repl_runtime {
struct MemoryFile {
  std::string type,path,content;
  std::optional<std::string> parent,rawContent;
  bool contentDiffersFromDisk=false;
};
struct RuntimeServices {
  event_loop::Loop& owner;
  QueryGuard& guard;
  submit::CommandQueue& queue;
  lifecycle::MessageState& messages;
  PresentationState& presentation;
  std::shared_ptr<file_state_stash::FileStateStash> readFileState;
  RuntimeServices(event_loop::Loop& loop,QueryGuard& queryGuard,
    submit::CommandQueue& commands,lifecycle::MessageState& messageState,
    PresentationState& view,
    std::shared_ptr<file_state_stash::FileStateStash> stash):
    owner(loop),guard(queryGuard),queue(commands),messages(messageState),
    presentation(view),readFileState(std::move(stash)){}
  std::function<event_loop::Deferred<event_loop::Unit>()> reverifyApiKey,
    shutdownDiagnostics;
  std::function<event_loop::Deferred<std::vector<MemoryFile>>()> getMemoryFiles;
  std::function<double()> nowMs;
  std::function<void(const std::string&)> logForDebugging;
  std::shared_ptr<submit::SubmitServices> submitServices;
  std::function<submit::PoetrySubmitRequest(const submit::CommandList&)>
    makeQueuedRequest;
  std::function<bool()> hasActiveLocalUi;
  std::function<TurnMessageRef(const std::string&,
    std::optional<bool>)> createUserMessage;
  std::function<void(std::shared_ptr<TurnAbort>)> setAbortController;
  std::function<std::shared_ptr<TurnAbort>()> getAbortController;
  std::function<event_loop::Deferred<event_loop::Unit>(TurnRequest)> onQuery;
  std::function<std::string()> mainLoopModel;
  std::function<void(std::exception_ptr)> onDetachedError;
};

struct FrozenTranscript {
  std::size_t messagesLength=0,streamingGadgetUsesLength=0;
};
struct TranscriptServices {
  std::function<void(const std::string&)> setHighlight;
  std::function<void()> clearPositions,cancelEditorTimer,requestRedraw;
};
class Runtime:public std::enable_shared_from_this<Runtime>{
public:
  explicit Runtime(RuntimeServices services,TranscriptServices transcript={});
  ~Runtime();
  void start();
  void stop();
  event_loop::Deferred<event_loop::Unit> stopAndWait();
  event_loop::Deferred<event_loop::Unit> onInit();
  event_loop::Deferred<event_loop::Unit> executeQueuedInput(
    submit::CommandList commands);
  bool handleIncomingPoetry(const std::string& content,bool isMeta=false);
  void observeQueuedCommands();
  void handleEnterTranscript();
  void handleExitTranscript();
  void setTranscriptScreen(bool value,bool virtualScrollActive);
  void setTranscriptSearch(std::string query);
  std::vector<TurnMessageRef> transcriptMessages()const;
  std::size_t transcriptStreamingGadgetUsesLength()const;
private:
  RuntimeServices services_;
  TranscriptServices transcript_;
  std::shared_ptr<submit::QueueReadiness> queueReadiness_;
  std::function<void()> unsubscribeQueue_;
  std::optional<FrozenTranscript> frozen_;
  std::string searchQuery_;
  bool started_=false,nowCheckScheduled_=false;
  bool transcriptScreen_=false,virtualScrollActive_=false,
    transcriptModeObserved_=false,inTranscriptObserved_=false;
  std::optional<event_loop::Deferred<event_loop::Unit>> stopPromise_;
  void scheduleNowCheck();
  void observeTranscriptMode();
  void reportDetached(std::exception_ptr error)const;
};

std::optional<std::string> stopHookSpinnerSuffix(
  const TurnMessagesRef& messages,bool isLoading);
} // namespace batbox::interactive::repl_runtime
