#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace batbox::git {
using event_loop::Deferred;
struct BasicsImports {
  std::function<std::int64_t()> nowMillis;
  std::function<void(const std::string&,const std::string&,
    const std::optional<Json>&)> logDiagnostic;
  std::function<std::string()> getCwd;
  std::function<std::optional<std::string>(const std::string&)> findGitRoot;
  std::function<Deferred<std::string>()> getStashedBranch,getStashedDefaultBranch;
  std::function<std::optional<std::string>(const std::string&)> whichSync;
};
class GitBasics {
public:
  GitBasics(event_loop::Loop& loop,BasicsImports imports);
  Deferred<bool> getIsGit();
  Deferred<bool> dirIsInGitRepo(const std::string& cwd);
  Deferred<std::string> getBranch();
  Deferred<std::string> getDefaultBranch();
  std::string gitExe();
private:
  event_loop::Loop& loop_;
  BasicsImports imports_;
  std::optional<Deferred<bool>> isGitStash_;
  std::optional<std::string> gitExeStash_;
  void owner()const;
};
std::function<std::optional<std::string>(const std::string&)> indigenousWhichSync(
  const std::string& pathEnvironment);
} // namespace batbox::git
