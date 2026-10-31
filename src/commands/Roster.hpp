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
const Json* findCommand(const std::string& name,const std::vector<Json>& commands);
bool hasCommand(const std::string& name,const std::vector<Json>& commands);
const Json& getCommand(const std::string& name,const std::vector<Json>& commands);
} // namespace batbox::commands
