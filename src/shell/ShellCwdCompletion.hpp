#pragma once
#include "shell/ManagedShell.hpp"

namespace batbox::shell {
struct CwdCompletionServices {
  bool windows=false;
  std::function<std::string(const std::string&)> posixPathToIndigenous;
  std::function<void()> cleanupAfterSandboxCommand;
  std::function<std::string(const std::string&)> readFileUtf8;
  std::function<std::string(const std::string&)> normalizeNFC;
  std::function<void(const std::string&,const std::string&)> setCwd;
  std::function<void()> invalidateFlightEnvStash;
  std::function<void(const std::string&,const std::string&)> onCwdChangedForHooks;
  std::function<void(const std::string&)> unlinkFile;
  std::function<void(std::exception_ptr)> onFailure;
};
// Complete working-directory and sandbox cleanup. The caller invokes
// this synchronously before publishing the result to the gadget consumer.
void completeShellCwd(const ExecResult* result,bool preventCwdChanges,
  bool shouldUseSandbox,const std::string& cwdFilePath,const std::string& oldCwd,
  const CwdCompletionServices& services);
} // namespace batbox::shell
