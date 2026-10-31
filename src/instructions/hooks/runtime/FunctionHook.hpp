#pragma once
#include "instructions/hooks/runtime/CombinedAbort.hpp"
#include "instructions/hooks/runtime/HookBlockingMessages.hpp"
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <map>
#include <optional>
#include <variant>

namespace batbox::instructions::hooks::runtime {
using FunctionHookReply=std::variant<bool,event_loop::Deferred<bool>>;
struct FunctionHook {
  Json fields=Json::object();
  std::optional<double> timeout;
  std::string errorMessage;
  std::function<FunctionHookReply(const Json&,std::stop_token)> callback;
};
struct FunctionHookCall {
  std::shared_ptr<const FunctionHook> hook;
  Json messages=Json::array();
  std::string hookName,gadgetUseID,hookEvent;
  double timeoutMs=0;
  std::optional<std::stop_token> signal;
};
struct FunctionHookResult {
  std::string outcome;
  std::shared_ptr<const FunctionHook> hook;
  std::optional<HookBlockingError> blockingError;
  std::optional<Json> message;
  FunctionHookResult(std::string value,std::shared_ptr<const FunctionHook> source,
    std::optional<HookBlockingError> blocking=std::nullopt,
    std::optional<Json> attachment=std::nullopt):outcome(std::move(value)),
      hook(std::move(source)),blockingError(std::move(blocking)),
      message(std::move(attachment)){}
};
struct FunctionHookServices {
  std::function<void(std::exception_ptr)> logError;
  std::function<Json(const Json&)> createAttachmentMessage;
  std::function<bool(std::exception_ptr)> isErrorInstance,isAbortError;
  std::function<std::string(std::exception_ptr)> errorMessage;
};
class FunctionHookHost {
public:
  explicit FunctionHookHost(event_loop::Loop& loop,FunctionHookServices services);
  event_loop::Deferred<FunctionHookResult> execute(FunctionHookCall call);
  std::size_t activeCount()const;
private:
  struct Run;
  struct State;
  std::shared_ptr<State> state_;
};
} // namespace batbox::instructions::hooks::runtime
