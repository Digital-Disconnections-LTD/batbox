#pragma once

#include "event_loop/AsyncContext.hpp"
#include <optional>
#include <string>

namespace batbox::interactive::submit {
std::optional<std::string> getWorkload();

// Always creates a fresh async-context boundary, including for undefined.
class WorkloadScope {
public:
  explicit WorkloadScope(std::optional<std::string> workload);
private:
  runtime::AsyncContextScope scope_;
};
} // namespace batbox::interactive::submit
