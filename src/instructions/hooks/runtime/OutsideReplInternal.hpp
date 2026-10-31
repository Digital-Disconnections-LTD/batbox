#pragma once
#include "instructions/hooks/runtime/OutsideRepl.hpp"
#include "instructions/hooks/runtime/HookAbort.hpp"
#include <memory>

namespace batbox::instructions::hooks::runtime {
struct OutsideRun : std::enable_shared_from_this<OutsideRun> {
  event_loop::Loop& loop;
  HookDispatch dispatch;
  ExecutorServices services;
  std::stop_token parentSignal;
  event_loop::Deferred<std::vector<HookResult>> output;
  std::string hookEvent,hookName,jsonInput;
  std::vector<HookResult> results;
  std::size_t remaining=0;
  OutsideRun(event_loop::Loop& owner,HookDispatch value,
    ExecutorServices imports,std::stop_token signal);
  void start();
  void afterMatching(const std::vector<MatchedHook>& hooks);
  void runOne(const MatchedHook& hook,std::size_t index);
  void runCallback(const MatchedHook& hook,std::size_t index);
  void runHttp(const MatchedHook& hook,std::size_t index);
  void runCommand(const MatchedHook& hook,std::size_t index);
  void finish(std::size_t index,HookResult result);
  void debug(const std::string& message,const std::string& level="info")const;
  void fail(std::exception_ptr error);
};
std::string exceptionMessage(std::exception_ptr error);
std::optional<double> timeoutSeconds(const Json& hook);
std::string stringField(const Json& value,const char* key);
std::optional<std::string> optionalString(const Json& value,const char* key);
bool syncBlocked(const Json& output);
std::optional<std::string> worktreePath(const Json& output);
} // namespace batbox::instructions::hooks::runtime
