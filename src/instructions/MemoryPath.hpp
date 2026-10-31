#pragma once
#include "runtime/JsonValue.hpp"
#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace batbox::instructions {
struct MemoryEnvironmentKeys {
  std::string disableAutoMemory="BATBOX_DISABLE_AUTO_MEMORY";
  std::string simpleMode="BATBOX_SIMPLE";
  std::string remoteMode="BATBOX_REMOTE";
  std::string remoteMemoryDir="BATBOX_REMOTE_MEMORY_DIR";
  std::string fullPathOverride="BATBOX_MEMORY_PATH_OVERRIDE";
};
struct MemoryPathServices {
  Json environment=Json::object();
  Json initialSettings=Json::object();
  Json settingsBySource=Json::object();
  std::string configHomeDir;
  std::string homeDirectory;
  std::string projectRoot;
  MemoryEnvironmentKeys keys;
  std::function<std::optional<std::string>(const std::string&)> findCanonicalGitRoot;
  // Bun's long-path hash is injected by the hosting runtime when applicable.
  std::function<std::string(const std::string&)> longPathHash;
};
struct MemoryPathStash {std::map<std::string,std::string> byProjectRoot;};
bool isAutoMemoryEnabled(const MemoryPathServices& services);
std::string getMemoryBaseDir(const MemoryPathServices& services);
std::optional<std::string> validateMemoryPath(const std::optional<std::string>& raw,
  bool expandTilde,const std::string& homeDirectory);
std::optional<std::string> getAutoMemPathOverride(const MemoryPathServices& services);
std::optional<std::string> getAutoMemPathSetting(const MemoryPathServices& services);
bool hasAutoMemPathOverride(const MemoryPathServices& services);
std::string sanitizeMemoryProjectPath(const std::string& name,
  const std::function<std::string(const std::string&)>& longPathHash={});
std::string getAutoMemPath(const MemoryPathServices& services,MemoryPathStash& stash);
std::string getAutoMemEntrypoint(const MemoryPathServices& services,MemoryPathStash& stash);
std::string getAutoMemDailyLogPath(const MemoryPathServices& services,MemoryPathStash& stash,
  std::chrono::system_clock::time_point date=std::chrono::system_clock::now());
bool isAutoMemPath(const std::string& absolutePath,const MemoryPathServices& services,
  MemoryPathStash& stash);
} // namespace batbox::instructions
