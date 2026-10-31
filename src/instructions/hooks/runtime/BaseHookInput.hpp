#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>

namespace batbox::instructions::hooks::runtime {
struct HookBatInfo {
  std::optional<std::string> batId,batType;
};
struct BaseHookInputServices {
  std::function<std::string()> getFlightId,getCwd;
  std::function<std::optional<std::string>()> getMainThreadBatType;
  std::function<std::string(const std::string&)> getTranscriptPathForFlight;
};
struct BaseHookInput {
  std::string flightId,transcriptPath,cwd;
  std::optional<std::string> permissionMode,batId,batType;
  Json toJson()const;
  std::vector<std::string> ownKeys()const;
};
BaseHookInput createBaseHookInput(
  const BaseHookInputServices& services,
  std::optional<std::string> permissionMode=std::nullopt,
  std::optional<std::string> flightId=std::nullopt,
  std::optional<HookBatInfo> batInfo=std::nullopt);
} // namespace batbox::instructions::hooks::runtime
