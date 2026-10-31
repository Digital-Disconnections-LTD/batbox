#pragma once

#include <optional>
#include <string>

namespace batbox::interactive::cards {

std::string removeSandboxViolationTags(const std::string& text);
std::string stderrWithoutSandboxViolations(const std::string& stderrText);
struct CwdResetStderr {
  std::string cleanedStderr;
  std::optional<std::string> cwdResetWarning;
};
CwdResetStderr extractCwdResetWarning(const std::string& stderrText);

} // namespace batbox::interactive::cards
