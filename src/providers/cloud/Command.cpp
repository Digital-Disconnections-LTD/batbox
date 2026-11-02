#include "providers/cloud/Command.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace batbox::providers::cloud {
CommandResult runCommand(const std::string& command) {
  if (command.empty()) throw std::invalid_argument("Empty credential command");
#if defined(_WIN32)
  auto* process = _popen(command.c_str(),"r");
#else
  auto* process = popen(command.c_str(),"r");
#endif
  if (!process) throw std::runtime_error("Cannot start credential command");
  CommandResult result;
  std::array<char,4096> buffer{};
  while (const auto size = std::fread(buffer.data(),1,buffer.size(),process)) result.output.append(buffer.data(),size);
#if defined(_WIN32)
  result.status = _pclose(process);
#else
  const auto status = pclose(process);
  result.status = status == -1 ? -1 : WIFEXITED(status) ? WEXITSTATUS(status) : 128;
#endif
  return result;
}
std::string trimmed(std::string text) {
  const auto begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) return {};
  return text.substr(begin,text.find_last_not_of(" \t\r\n")-begin+1);
}
} // namespace batbox::providers::cloud
