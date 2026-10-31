#pragma once
#include "cwd/State.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::cwd {
struct SetCwdServices {
  std::function<std::string()> fsCwd;
  std::function<std::string(const std::string&)> realpathSync;
};
// Resolve relative inputs against relativeTo or the filesystem cwd.
// Physically resolve both absolute inputs and resolved relative inputs.
void setCwd(State& state,const std::string& path,
  const std::optional<std::string>& relativeTo,const SetCwdServices& services);
SetCwdServices indigenousSetCwdServices();
std::string resolvePosix(const std::string& base,const std::string& path);
} // namespace batbox::cwd
