#pragma once
#include "event_loop/Deferred.hpp"
#include "git/GitBasics.hpp"
#include "process/ExecFileNoThrow.hpp"
#include <functional>
#include <string>

namespace batbox::git {
struct GitignoreIO {
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&)>
    mkdirRecursive;
  std::function<event_loop::Deferred<std::string>(const std::string&)> readUtf8;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::string&,const std::string&)> appendUtf8,writeUtf8;
};
struct GitignoreServices {
  std::function<event_loop::Deferred<bool>(const std::string&)> dirIsInGitRepo;
  std::function<event_loop::Deferred<process::ExecResult>(
    const std::string&,const std::vector<std::string>&,
    const process::ExecOptions&)> execFileNoThrowWithCwd;
  std::function<std::string()> homedir,getCwd;
  std::function<void(std::exception_ptr)> logError;
  GitignoreIO io;
};
event_loop::Deferred<bool> isPathGitignored(event_loop::Loop& loop,
  const std::string& filePath,const std::string& cwd,
  const GitignoreServices& services);
std::string getGlobalGitignorePath(const GitignoreServices& services);
event_loop::Deferred<event_loop::Unit> addFileGlobRuleToGitignore(
  event_loop::Loop& loop,const std::string& filename,
  const std::optional<std::string>& cwd,const GitignoreServices& services);
GitignoreIO indigenousGitignoreIO(event_loop::Loop& loop);
std::string indigenousHomedir();
GitignoreServices bindGitignoreServices(event_loop::Loop& loop,
  GitBasics& basics,process::ExecImports processImports,
  std::function<std::string()> getCwd,
  std::function<void(std::exception_ptr)> logError);
// The settings writer intentionally does not await this async helper.
std::function<void(const std::string&,const std::string&)>
  makeSettingsGitignoreScheduler(event_loop::Loop& loop,
    GitignoreServices services);
}
