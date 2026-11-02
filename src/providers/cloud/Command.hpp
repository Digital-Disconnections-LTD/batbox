#pragma once
#include <string>

namespace batbox::providers::cloud {
struct CommandResult {
  int status = 0;
  std::string output;
};
// Runs a shell command and captures its standard output.
CommandResult runCommand(const std::string& command);
std::string trimmed(std::string text);
} // namespace batbox::providers::cloud
