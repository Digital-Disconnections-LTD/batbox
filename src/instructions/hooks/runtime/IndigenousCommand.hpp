#pragma once
#include "instructions/hooks/runtime/OutsideRepl.hpp"
#include <map>

namespace batbox::instructions::hooks::runtime {
// Run POSIX commands through the shell. Preserve command input/output bytes
// until UTF-8 decoding, and settle the result on the caller's owner loop.
struct IndigenousCommandConfig {
  std::string shellPath="/bin/sh";
  std::string defaultShell="bash";
  std::function<std::string()> getProjectRoot,getCwd,getOriginalCwd;
  std::function<std::map<std::string,std::string>()> subprocessEnv;
  std::function<event_loop::Deferred<bool>(const std::string&)> pathExists;
  std::function<std::optional<std::string>()> shellPrefix;
  std::function<std::string(const std::string&)> getPluginDataDir;
  std::function<Json(const std::string&)> loadPluginOptions;
  std::function<std::string(const std::string&,const Json&)> substituteUserConfig;
  std::function<event_loop::Deferred<std::string>(const std::string&,std::size_t)>
    getHookEnvFilePath;
  std::string projectDirVariable="BATBOX_PROJECT_DIR";
  std::string pluginRootVariable="BATBOX_PLUGIN_ROOT";
  std::string pluginDataVariable="BATBOX_PLUGIN_DATA";
  std::string pluginOptionPrefix="BATBOX_PLUGIN_OPTION_";
  std::string environmentFileVariable="BATBOX_ENV_FILE";
  double defaultTimeoutMs=600000;
  std::function<void(const std::string&,const std::string&)> logDebug;
  std::function<void(const std::string&,const Json&)> logDiagnostics;
  std::function<double()> nowMillis;
};
std::function<event_loop::Deferred<CommandOutcome>(CommandRequest)>
indigenousCommandHook(event_loop::Loop& loop,IndigenousCommandConfig config);
// Synchronous command-hook adapter using cross-platform preparation. The
// application provider routes configured async hooks through
// bindIndigenousFullCommandHook and its AsyncHookRoster.
std::function<event_loop::Deferred<CommandOutcome>(CommandRequest)>
indigenousPreparedCommandHook(event_loop::Loop& loop,IndigenousCommandConfig config);
} // namespace batbox::instructions::hooks::runtime
