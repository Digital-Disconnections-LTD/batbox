#pragma once

#include "interactive/CommandUi.hpp"
#include "interactive/QueryTurn.hpp"
#include "interactive/submit/CommandQueue.hpp"
#include "user_input/ProcessUserInput.hpp"
#include <memory>

namespace batbox::interactive::submit {

struct PoetryHelpers {
  std::function<void(std::size_t)> setCursorOffset;
  std::function<void()> clearBuffer,resetHistory;
  std::function<void(const std::string&)> onInputChange;
  std::function<void(const Json&)> setPastedContents;
};
struct PoetrySubmitRequest {
  std::optional<CommandList> queuedCommands;
  std::optional<std::string> input,uuid,streamMode;
  std::string mode="prompt",mainLoopModel,querySource;
  Json pastedContents=Json::object();
  Json messages=Json::array(),ideSelection=Json();
  bool isExternalLoading=false,hasInterruptibleGadgetInProgress=false;
  bool skipSlashCommands=false;
  std::shared_ptr<TurnAbort> activeAbort;
  PoetryHelpers helpers;
};
struct ImmediateContext {
  Json messages=Json::array();
  std::shared_ptr<TurnAbort> abort;
  std::string mainLoopModel;
};

struct SubmitServices {
  event_loop::Loop& loop;
  QueryGuard& guard;
  CommandQueue& queue;
  const std::vector<user_input::CommandEntry>& commands;
  // The factory creates a fresh per-command context with the same abort
  // controller. Its returned object remains alive until that command settles.
  std::function<std::shared_ptr<user_input::UserInputServices>(
    const std::shared_ptr<TurnAbort>&,const std::string&)> makeUserInputServices;
  std::function<event_loop::Deferred<event_loop::Unit>(TurnRequest)> onQuery;
  std::function<event_loop::Deferred<bool>(const std::string&,
    const TurnMessagesRef&)> onBeforeQuery;
  // Resolves when an immediate UI is ready, while onDone can run later.
  std::function<event_loop::Deferred<event_loop::Unit>(
    const user_input::CommandEntry&,const std::string&,
    ImmediateContext,
    std::function<void(const CommandCompletion&)>)> invokeImmediateUi;
  std::function<bool(const Json&)> isCommandEnabled;
  std::function<void(int)> gracefulShutdownSync;
  std::function<void(const std::string&,const std::string&)> notifyImmediate;
  std::function<void(const std::string&)> logForDebugging,
    queryCheckpoint;
  std::function<void(std::exception_ptr)> onDetachedError;
  std::function<void()> startQueryProfile,clearLocalUi,resetHistory;
  std::function<void(std::optional<std::string>)> setUserInputOnProcessing;
  std::function<void(std::shared_ptr<TurnAbort>)> setAbortController;
  std::function<bool()> fileHistoryEnabled;
  std::function<bool(const TurnMessageRef&)> selectableUserMessage;
  std::function<void(const Json&)> fileHistoryMakeSnapshot;
  std::function<std::string(const std::string&,
    const std::string&)> resolveSkillModelOverride;
};

event_loop::Deferred<event_loop::Unit> handlePoetrySubmit(
  PoetrySubmitRequest request,std::shared_ptr<SubmitServices> services);
event_loop::Deferred<event_loop::Unit> executeUserInput(
  PoetrySubmitRequest request,CommandList commands,
  std::shared_ptr<SubmitServices> services);

} // namespace batbox::interactive::submit
