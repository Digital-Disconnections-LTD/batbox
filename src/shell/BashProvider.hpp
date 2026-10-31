#pragma once
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace batbox::shell_provider {
struct BuildOptions {
  std::string id;
  std::optional<std::string> sandboxTmpDir;
  bool useSandbox=false;
};
struct BuildResult {std::string commandString,cwdFilePath;};
struct Services {
  std::function<std::shared_future<std::optional<std::string>>(const std::string&)> createSnapshot;
  std::function<std::optional<std::string>()> flightEnvironmentScript;
  std::function<std::map<std::string,std::string>()> flightEnvVars;
  std::function<bool()> tmuxGadgetUsed;
  std::function<void()> ensureTmuxSocket;
  std::function<std::optional<std::string>()> tmuxEnvironment;
  std::function<std::string(const std::string&)> indigenousToPosixPath;
  std::function<std::string(const std::string&,const std::string&)> indigenousJoin;
  std::function<std::string()> operatingSystemTempDir;
  std::function<bool(const std::string&)> snapshotAccessible;
  bool windows=false;
  bool shellPrefixConfigured=false;
  std::string shellPrefix;
  bool internalOperator=false;
  bool skipSnapshot=false;
};
class BashProvider {
public:
  BashProvider(std::string shellPath,Services services);
  BuildResult buildExecCommand(const std::string& command,const BuildOptions& options);
  std::vector<std::string> getSpawnArgs(const std::string& commandString)const;
  std::map<std::string,std::string> getEnvironmentOverrides(const std::string& command);
  const std::string& shellPath()const{return shellPath_;}
  bool detached()const{return true;}
private:
  std::string shellPath_;
  Services services_;
  std::shared_future<std::optional<std::string>> snapshotFuture_;
  mutable std::mutex mutex_;
  std::optional<std::string> lastSnapshotPath_,currentSandboxTmpDir_;
};
std::optional<std::string> getDisableExtglobCommand(const std::string& shellPath,
  bool shellPrefixConfigured);
} // namespace batbox::shell_provider
