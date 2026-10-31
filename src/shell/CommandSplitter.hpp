#pragma once
#include <functional>
#include <string>
#include <vector>

namespace batbox::shell_split {
using SaltProvider=std::function<std::string()>;
std::vector<std::string> splitCommandWithOperators(const std::string& command,
  const SaltProvider& saltProvider={});
std::vector<std::string> splitCommandDeprecated(const std::string& command,
  const SaltProvider& saltProvider={});
std::vector<std::string> filterControlOperators(const std::vector<std::string>& parts);
bool isStaticRedirectTarget(const std::string& target);
} // namespace batbox::shell_split
