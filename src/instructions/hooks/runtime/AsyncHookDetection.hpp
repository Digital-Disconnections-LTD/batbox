#pragma once
#include "instructions/hooks/runtime/BackgroundHook.hpp"
#include "instructions/hooks/runtime/OutsideRepl.hpp"

namespace batbox::instructions::hooks::runtime {
struct AsyncDetectionServices {
  std::function<void(const std::string&)> logDebug;
  std::function<Json(const std::string&)> parseJson;
  std::function<std::string(const Json&)> stringifyJson;
  std::function<std::string(std::exception_ptr)> stringifyThrown;
  std::function<bool(BackgroundHookRequest)> executeBackground;
};
// The stdout listener detects the first-line async protocol from execCommandHook.
// Poetry-request handling is separate.
class AsyncHookDetection {
public:
  AsyncHookDetection(event_loop::Loop& loop,BackgroundHookRequest request,
    AsyncDetectionServices services,bool forceSyncExecution=false);
  void onStdout(const std::string& data);
  void onStderr(const std::string& data);
  event_loop::Deferred<CommandOutcome> asyncResult()const{return asyncResult_;}
  bool initialResponseChecked()const{return initialResponseChecked_;}
  bool transferred()const{return transferred_;}
  const std::string& stdoutText()const{return stdout_;}
  const std::string& stderrText()const{return stderr_;}
  const std::string& outputText()const{return output_;}
private:
  event_loop::Loop& loop_;
  BackgroundHookRequest request_;
  AsyncDetectionServices services_;
  bool forceSyncExecution_=false,initialResponseChecked_=false,transferred_=false;
  std::string stdout_,stderr_,output_;
  event_loop::Deferred<CommandOutcome> asyncResult_;
};
} // namespace batbox::instructions::hooks::runtime
