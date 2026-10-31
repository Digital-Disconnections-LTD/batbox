#pragma once
#include "cli/structured/ControlResponses.hpp"
#include "event_loop/Deferred.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::cli::structured {
struct ProcessLineServices {
  std::function<void(const std::string&,const std::string&)> setEnvironment;
  std::function<void(const std::string&,const Json&)> logForDebugging;
  std::function<void(const std::string&,const std::string&)> notifyCommandLifecycle;
  std::function<event_loop::Deferred<event_loop::Unit>(const Json&)>
    unexpectedResponseCallback;
  std::function<void(const std::string&)> onControlRequestResolved;
  std::function<void(const std::string&)> fatalInput;
  bool replayUserMessages=false;
};

event_loop::Deferred<std::optional<Json>> processLine(
  event_loop::Loop& loop,const std::string& line,ControlResponses& control,
  const ProcessLineServices& services);
}
