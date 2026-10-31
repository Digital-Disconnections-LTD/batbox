#pragma once
#include "shell/SnapshotContent.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace batbox::shell_snapshot {
struct RunServices {
  ContentServices content;
  std::function<std::string()> configHome;
  std::function<std::string()> homeDirectory;
  std::function<std::int64_t()> nowMillis;
  std::function<std::string()> randomId;
  std::function<std::map<std::string,std::string>()> subprocessEnvironment;
  std::function<void(std::function<void()>)> registerCleanup;
  std::function<void(const std::string&)> diagnostic;
  bool dontInheritEnvironment=false;
};
std::optional<std::string> createAndSaveSnapshot(const std::string& shellPath,
  const RunServices& services);
} // namespace batbox::shell_snapshot
