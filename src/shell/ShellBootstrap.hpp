#pragma once
#include <functional>
#include <optional>
#include <string>

namespace batbox::shell {
struct DiscoveryServices {
  std::optional<std::string> shellOverride;
  std::optional<std::string> preferredShell;
  std::function<bool(const std::string&)> isExecutable;
  std::function<std::optional<std::string>(const std::string&)> which;
  std::function<void(const std::string&)> debug;
};
// The shell for Bash commands: BATBOX_SHELL when it names an executable bash
// or zsh, then SHELL when it does, then bash or zsh from PATH and the usual
// install folders, then /bin/sh. Throws when there is no shell at all.
std::string findSuitableShell(const DiscoveryServices& services);
// Indigenous bindings for the executable check and the PATH lookup.
bool indigenousIsExecutable(const std::string& shellPath);
std::function<std::optional<std::string>(const std::string&)>
indigenousWhich(const std::string& pathEnvironment);
} // namespace batbox::shell
