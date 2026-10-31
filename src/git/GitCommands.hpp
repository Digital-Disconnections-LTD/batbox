#pragma once
#include "event_loop/Deferred.hpp"
#include "process/ExecFileNoThrow.hpp"
#include <functional>
#include <string>
#include <vector>

namespace batbox::git {
struct CommandImports {
  std::function<std::string()> gitExe;
  std::function<event_loop::Deferred<process::ExecResult>(
    const std::string&,const std::vector<std::string>&,
    const process::ExecOptions&)> execFileNoThrow;
};
struct GitFileStatus {
  std::vector<std::string> tracked,untracked;
};
class GitCommands {
public:
  GitCommands(event_loop::Loop& loop,CommandImports imports);
  event_loop::Deferred<bool> getIsHeadOnRemote();
  event_loop::Deferred<bool> hasUnpushedCommits();
  event_loop::Deferred<bool> getIsClean(bool ignoreUntracked=false);
  event_loop::Deferred<std::vector<std::string>> getChangedFiles();
  event_loop::Deferred<GitFileStatus> getFileStatus();
private:
  event_loop::Loop& loop_;
  CommandImports imports_;
  event_loop::Deferred<process::ExecResult> run(const std::vector<std::string>& args);
};
} // namespace batbox::git
