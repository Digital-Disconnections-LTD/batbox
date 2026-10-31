#pragma once
#include "interactive/navigation/DialogFocus.hpp"
#include "interactive/submit/CommandQueue.hpp"

namespace batbox::interactive::navigation {
struct GadgetConfirmation {
  std::function<void()> onAbort;
};
struct PendingPoetry {
  std::function<void(std::exception_ptr)> reject;
};
struct CancelState {
  Dialog focusedInputDialog;
  std::string streamMode;
  bool activeRemoteMode=false;
  std::shared_ptr<TurnAbort> abortController;
  std::vector<std::shared_ptr<GadgetConfirmation>> gadgetConfirms;
  std::vector<std::shared_ptr<PendingPoetry>> poetryEntries;
};
struct CancelServices {
  event_loop::Loop& owner;
  QueryTurnState& turn;
  lifecycle::MessageState& messages;
  lifecycle::LoadingState& loading;
  PresentationState& presentation;
  CancelServices(event_loop::Loop& loop,QueryTurnState& turnState,
    lifecycle::MessageState& messageState,lifecycle::LoadingState& loadingState,
    PresentationState& view):owner(loop),turn(turnState),messages(messageState),
    loading(loadingState),presentation(view){}
  std::function<void(const std::string&)> logForDebugging;
  std::function<bool(const std::string&)> feature;
  std::function<void()> pauseProactive,cancelRemote;
  std::function<TurnMessageRef(const std::string&)> createAssistantMessage;
  std::function<void(std::optional<double>)> snapshotOutputTokensForTurn;
  std::function<void()> publishGadgetConfirmQueue,publishPoetryQueue;
  std::function<void(std::shared_ptr<TurnAbort>)> publishAbortController;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const TurnMessagesRef&,bool)> mrOnTurnComplete;
  std::function<void(std::exception_ptr)> onDetachedError;
};
void onCancel(CancelState& state,const CancelServices& services);

struct QueueCancelServices {
  submit::CommandQueue& queue;
  explicit QueueCancelServices(submit::CommandQueue& commands):queue(commands){}
  std::function<double()> nowMs;
  std::function<void(const std::string&)> setInputValue,setInputMode;
  std::function<void(const std::function<Json(const Json&)>&)>
    updatePastedContents;
};
void handleQueuedCommandOnCancel(const std::string& inputValue,
  const QueueCancelServices& services);
} // namespace batbox::interactive::navigation
