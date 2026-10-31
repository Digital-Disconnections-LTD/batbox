#pragma once
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::shell_snapshot {
struct RipgrepCommand {
  std::string path;
  std::vector<std::string> args;
  std::optional<std::string> argv0;
};
struct Integration {std::string type,snippet;};
struct ContentServices {
  std::function<RipgrepCommand()> ripgrepCommand;
  std::function<bool()> hasEmbeddedSearchGadgets;
  std::function<std::string()> embeddedSearchGadgetsBinaryPath;
  std::function<std::string()> pathValue;
  std::function<std::optional<std::string>()> cygwinPathValue;
  bool windows=false;
};
std::string createArgv0ShellFunction(const std::string& funcName,
  const std::string& argv0,const std::string& binaryPath,
  const std::vector<std::string>& prependArgs={});
Integration createRipgrepShellIntegration(const ContentServices& services);
std::optional<std::string> createFindGrepShellIntegration(const ContentServices& services);
std::string getConfigFile(const std::string& shellPath,const std::string& homeDirectory);
std::string getUserSnapshotContent(const std::string& configFile);
std::string getBatboxSnapshotContent(const ContentServices& services);
std::string getSnapshotScript(const std::string& shellPath,
  const std::string& snapshotFilePath,bool configFileExists,
  const std::string& homeDirectory,const ContentServices& services);
} // namespace batbox::shell_snapshot
