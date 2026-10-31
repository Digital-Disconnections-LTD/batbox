#pragma once

#include "runtime/JsonValue.hpp"
#include <optional>
#include <string>

namespace batbox::interactive::cards {

struct BashResultCard {
  bool image = false;
  std::optional<std::string> stdoutText;
  std::optional<std::string> stderrText;
  std::optional<std::string> cwdResetWarning;
  std::optional<std::string> noOutputText;
  bool backgroundHint = false;
  std::optional<double> timeoutMs;
  bool verbose = false;
};

// Input is the shell gadget's result object. Output and time display consume
// the selected fields.
BashResultCard buildBashResultCard(const Json& content,
                                   bool verbose,
                                   std::optional<double> timeoutMs = {});

} // namespace batbox::interactive::cards
