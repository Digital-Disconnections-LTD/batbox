#pragma once

#include <optional>
#include <string>

namespace batbox::interactive::cards {

struct CwdResetStderr {
  std::string cleanedStderr;
  std::optional<std::string> cwdResetWarning;
};
CwdResetStderr extractCwdResetWarning(const std::string& stderrText);

} // namespace batbox::interactive::cards
