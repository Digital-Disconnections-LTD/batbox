#pragma once
#include "runtime/JsonValue.hpp"
#include <optional>
#include <string>
#include <vector>

namespace batbox::poetryEntries {
std::vector<std::string> prependBullets(const Json& items);
std::optional<std::string> getLanguageSection(
  const std::optional<std::string>& language);
std::optional<std::string> getOutputStyleSection(const Json& config);
std::optional<std::string> getMcpInstructions(const Json& clients);
std::optional<std::string> getMcpInstructionsSection(const Json& clients);
std::string getSimpleIntroSection(const Json& outputStyleConfig,
  const std::string& cyberRiskInstruction);
}
