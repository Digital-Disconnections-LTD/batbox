#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::cli::structured {
struct ProcessLineServices {
  std::function<void(const std::string&,const std::string&)> setEnvironment;
  std::function<void(const std::string&,const Json&)> logForDebugging;
  std::function<void(const std::string&,const std::string&)> notifyCommandLifecycle;
  std::function<void(const std::string&)> fatalInput;
};

event_loop::Deferred<std::optional<Json>> processLine(
  event_loop::Loop& loop,const std::string& line,
  const ProcessLineServices& services);
}
