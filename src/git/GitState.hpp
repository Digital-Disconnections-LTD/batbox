#pragma once
#include "event_loop/Deferred.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::git {
struct GitRepoState {
  std::string commitHash,branchName;
  std::optional<std::string> remoteUrl;
  bool isHeadOnRemote=false,isClean=false;
  std::size_t worktreeCount=0;
};
struct GitStateImports {
  std::function<event_loop::Deferred<std::string>()> getHead,getBranch;
  std::function<event_loop::Deferred<std::optional<std::string>>()> getRemoteUrl;
  std::function<event_loop::Deferred<bool>()> getIsHeadOnRemote,getIsClean;
  std::function<event_loop::Deferred<std::size_t>()> getWorktreeCount;
};
event_loop::Deferred<std::optional<GitRepoState>> getGitState(
  event_loop::Loop& loop,const GitStateImports& imports);
} // namespace batbox::git
