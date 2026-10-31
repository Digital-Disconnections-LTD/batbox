#pragma once
#include "event_loop/Deferred.hpp"
#include "instructions/hooks/InstructionsLoaded.hpp"
#include "runtime/JsonValue.hpp"
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace batbox::instructions::hooks::runtime {
struct FunctionHook;
enum class HookKind { Callback,Poetry,Bat,Function,Http,Command };
struct HookResult {
  HookResult(std::string label={},std::string text={},bool success=false,
    bool isBlocked=false,std::optional<std::vector<std::string>> paths=std::nullopt,
    std::optional<std::string> message=std::nullopt)
    :command(std::move(label)),output(std::move(text)),succeeded(success),
      blocked(isBlocked),watchPaths(std::move(paths)),systemMessage(std::move(message)){}
  std::string command,output;
  bool succeeded=false,blocked=false;
  std::optional<std::vector<std::string>> watchPaths;
  std::optional<std::string> systemMessage;
  Json toJson()const;
};
struct CommandOutcome {
  std::string stdoutText,stderrText,output;
  int status=0;
  bool aborted=false,backgrounded=false;
};
struct HttpOutcome {
  bool aborted=false,ok=false;
  std::optional<std::string> error;
  std::optional<int> statusCode;
  std::string body;
};
struct ParsedOutput {
  std::optional<Json> json;
  std::optional<std::string> validationError;
};
struct CallbackRequest {
  Json hookInput;
  std::string uuid;
  std::stop_token signal;
  std::size_t index=0;
};
struct CommandRequest {
  Json hook;
  std::string hookEvent,hookName,jsonInput,uuid;
  std::size_t index=0;
  std::optional<std::string> pluginRoot,pluginId;
  std::stop_token signal;
  std::optional<std::string> skillRoot;
};
struct HttpRequest {
  Json hook;
  std::string hookEvent,jsonInput;
  std::stop_token signal;
};
struct MatchedHook {
  HookKind kind=HookKind::Command;
  Json hook=Json::object();
  std::optional<std::string> pluginRoot,pluginId;
  std::optional<std::string> skillRoot;
  std::string hookSource="settings";
  std::function<event_loop::Deferred<Json>(CallbackRequest)> callback;
  std::shared_ptr<const FunctionHook> function;
};
struct ExecutorServices {
  std::function<bool()> simpleMode,allHooksDisabled,skipDueToTrust;
  std::function<Json()> getAppState;
  std::function<std::string()> getFlightId,randomUUID;
  std::function<event_loop::Deferred<std::vector<MatchedHook>>(
    const std::string& flightId,const std::string& hookEvent,const Json& hookInput)>
    getMatchingHooks;
  std::function<event_loop::Deferred<std::vector<MatchedHook>>(
    const Json& appState,const std::string& flightId,
    const std::string& hookEvent,const Json& hookInput)> getMatchingHooksWithAppState;
  std::function<event_loop::Deferred<CommandOutcome>(CommandRequest)> execCommandHook;
  std::function<event_loop::Deferred<HttpOutcome>(HttpRequest)> execHttpHook;
  std::function<ParsedOutput(const std::string&)> parseHookOutput,
    parseHttpHookOutput;
  std::function<void(const std::string&,const std::string&)> logDebug;
  std::function<void(const std::string&)> logError;
};
event_loop::Deferred<std::vector<HookResult>> executeHooksOutsideRepl(
  event_loop::Loop& loop,const HookDispatch& dispatch,
  const ExecutorServices& services,std::stop_token signal={});
// Existing InstructionsLoaded boundary expects JSON array from this executor.
std::function<event_loop::Deferred<Json>(HookDispatch)> bindOutsideReplExecutor(
  event_loop::Loop& loop,ExecutorServices services,std::stop_token signal={});
bool isAsyncHookJsonOutput(const Json& json);
bool isSyncHookJsonOutput(const Json& json);
} // namespace batbox::instructions::hooks::runtime
