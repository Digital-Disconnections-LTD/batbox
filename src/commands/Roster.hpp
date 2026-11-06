#pragma once
#include "runtime/JsonValue.hpp"
#include <optional>
#include <string>
#include <vector>

namespace batbox::commands {
struct ParsedSlashCommand {
  std::string commandName;
  std::string args;
  bool isMcp=false;
};

std::optional<ParsedSlashCommand> parseSlashCommand(const std::string& input);
bool looksLikeCommand(const std::string& name);
std::string getCommandName(const Json& command);
} // namespace batbox::commands
