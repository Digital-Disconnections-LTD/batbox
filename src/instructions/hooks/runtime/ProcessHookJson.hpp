#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::instructions::hooks::runtime {
struct ProcessHookJsonInput {
  Json json=Json::object();
  std::string command,hookName,gadgetUseID,hookEvent;
  std::optional<std::string> expectedHookEvent,stdoutText,stderrText;
  std::optional<double> exitCode,durationMs;
};
struct ProcessHookJsonResult {
  Json fields=Json::object();
  std::vector<std::string> ownUndefinedKeys;
};
struct ProcessHookJsonServices {
  // Attachment factories receive explicitly undefined keys separately from JSON
  // serialization, so callers can preserve object shape where needed.
  std::function<Json(const Json&,const std::vector<std::string>&)>
    createAttachmentMessage;
};
ProcessHookJsonResult processHookJSONOutput(const ProcessHookJsonInput& input,
  const ProcessHookJsonServices& services);
} // namespace batbox::instructions::hooks::runtime
