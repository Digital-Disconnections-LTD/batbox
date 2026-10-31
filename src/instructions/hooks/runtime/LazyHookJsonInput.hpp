#pragma once
#include "runtime/JsonValue.hpp"
#include <exception>
#include <functional>
#include <optional>
#include <string>

namespace batbox::instructions::hooks::runtime {
struct HookJsonInputResult {
  bool ok=false;
  std::string value;
  std::exception_ptr error;
};
class LazyHookJsonInput {
public:
  LazyHookJsonInput(Json input,std::string hookName,
    std::function<std::string(const Json&)> stringify,
    std::function<void(const std::string&,std::exception_ptr)> logError);
  const HookJsonInputResult& get();
private:
  Json input_;
  std::string hookName_;
  std::function<std::string(const Json&)> stringify_;
  std::function<void(const std::string&,std::exception_ptr)> logError_;
  std::optional<HookJsonInputResult> result_;
};
} // namespace batbox::instructions::hooks::runtime
