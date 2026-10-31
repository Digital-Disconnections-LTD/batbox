#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>
namespace batbox::bash_gadget {
using SplitCommand=std::function<std::vector<std::string>(const std::string&)>;
std::string stripEmptyLines(const std::string& content);
bool isImageOutput(const std::string& content);
std::optional<Json> parseDataUri(const std::string& content);
Json buildImageGadgetResult(const std::string& stdoutText,const std::string& gadgetUseID);
Json formatOutput(const std::string& content,std::size_t maxOutputLength);
Json interpretCommandResult(const std::string& command,int exitCode,
  const std::string& stdoutText,const std::string& stderrText,const SplitCommand& splitCommand);
std::optional<std::string> detectBlockedSleepPattern(const std::string& command,const SplitCommand& splitCommand);
Json isSearchOrReadBashCommand(const std::string& command,const SplitCommand& splitWithOperators);
bool isSilentBashCommand(const std::string& command,const SplitCommand& splitWithOperators);
} // namespace batbox::bash_gadget
