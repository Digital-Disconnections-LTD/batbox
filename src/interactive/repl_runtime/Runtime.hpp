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
using LocalCommandRef=std::shared_ptr<Json>;
using LocalCommands=std::vector<LocalCommandRef>;
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
  LocalCommands initialLocalCommands;
  std::function<bool(const LocalCommandRef&)> isRemoteSafeCommand;
  std::function<void(const LocalCommands&)> publishLocalCommands;
  std::function<void()> recordUserActivity,startBackgroundHousekeeping;
  std::function<std::size_t()> getSubmitCount;
  std::function<void(bool)> updateLastInteractionTime;
  std::function<void(std::exception_ptr)> onDetachedError;
};

struct FrozenTranscript {
  std::size_t messagesLength=0,streamingGadgetUsesLength=0;
};
struct TranscriptServices {
  std::function<void(const std::string&)> setHighlight;
  std::function<void()> clearPositions,cancelEditorTimer,requestRedraw;
};
struct IdleServices {
  std::function<double()> nowMs,getLastInteractionTime,
    messageIdleThresholdMs,getTotalInputTokens,
    idleTokenThreshold,idleMinutesThreshold;
  std::function<bool()> focusedDialogPresent,idleReturnDismissed;
  std::function<std::string()> idleReturnMode;
  std::function<std::string(double)> formatTokens;
  std::function<void(const std::string&,const std::string&)> sendIdlePoetry;
  std::function<void(const std::string&,const std::string&)> addIdleReturnHint;
  std::function<void()> removeIdleReturnHint;
};

class Runtime:public std::enable_shared_from_this<Runtime>{
public:
  explicit Runtime(RuntimeServices services,TranscriptServices transcript={},
    IdleServices idle={});
  ~Runtime();
  void start();
  void stop();
  event_loop::Deferred<event_loop::Unit> stopAndWait();
  event_loop::Deferred<event_loop::Unit> onInit();
  event_loop::Deferred<event_loop::Unit> executeQueuedInput(
    submit::CommandList commands);
  bool handleIncomingPoetry(const std::string& content,bool isMeta=false);
  void handleRemoteInit(const std::vector<std::string>& remoteSlashCommands);
  void observeInputActivity(const std::string& input,std::size_t submitCount);
  void observeQueuedCommands();
  void observeIdleEffects(double lastQueryCompletionTime,bool isLoading,
    bool hasLocalUi,std::size_t submitCount);
  void handleEnterTranscript();
  void handleExitTranscript();
  void setTranscriptScreen(bool value,bool virtualScrollActive);
  void setTranscriptSearch(std::string query,bool open,
    std::size_t count,std::size_t current);
  void setTranscriptDumpMode(bool value){dumpMode_=value;}
  void setTranscriptEditorStatus(std::string value){editorStatus_=std::move(value);}
  std::vector<TurnMessageRef> transcriptMessages()const;
  std::size_t transcriptStreamingGadgetUsesLength()const;
  const std::optional<FrozenTranscript>& frozenTranscript()const{return frozen_;}
  const LocalCommands& localCommands()const{return localCommands_;}
  bool transcriptScreen()const{return transcriptScreen_;}
  std::uint64_t editorGeneration()const{return editorGeneration_;}
  const std::string& editorStatus()const{return editorStatus_;}
private:
  RuntimeServices services_;
  TranscriptServices transcript_;
  IdleServices idle_;
  LocalCommands localCommands_;
  std::shared_ptr<submit::QueueReadiness> queueReadiness_;
  std::function<void()> unsubscribeQueue_;
  std::optional<std::pair<std::string,std::size_t>> activitySnapshot_;
  struct IdleSnapshot {
    double completion=0;
    bool loading=false,localUi=false;
    std::size_t submitCount=0;
  };
  std::optional<IdleSnapshot> idlePoetrySnapshot_,idleReturnSnapshot_;
  event_loop::Loop::TimerHandle idlePoetryTimer_,idleReturnTimer_;
  std::optional<FrozenTranscript> frozen_;
  std::string searchQuery_,editorStatus_;
  std::size_t searchCount_=0,searchCurrent_=0;
  std::uint64_t editorGeneration_=0;
  bool started_=false,nowCheckScheduled_=false;
  bool idleReturnEffectActive_=false;
  bool transcriptScreen_=false,virtualScrollActive_=false,
    transcriptModeObserved_=false,inTranscriptObserved_=false,
    searchOpen_=false,dumpMode_=false;
  std::optional<event_loop::Deferred<event_loop::Unit>> stopPromise_;
  void scheduleNowCheck();
  void observeTranscriptMode();
  void cancelIdleEffects();
  void reportDetached(std::exception_ptr error)const;
};

std::optional<std::string> stopHookSpinnerSuffix(
  const TurnMessagesRef& messages,bool isLoading);
} // namespace batbox::interactive::repl_runtime
