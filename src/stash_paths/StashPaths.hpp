#pragma once
#include <functional>
#include <string>

namespace batbox::stash_paths {

struct Imports {
  // env-paths('batbox-cli').stash, evaluated once when this module is bound.
  std::function<std::string()> stashDirectory;
  std::function<std::string()> cwd;
  std::function<std::string(const std::string&,const std::string&)> join;
};

std::string sanitizePath(const std::string& name);
std::string getProjectDir(const std::string& cwd);

class StashPaths {
public:
  explicit StashPaths(Imports imports);
  std::string baseLogs() const;
  std::string errors() const;
  std::string messages() const;
  std::string mcpLogs(const std::string& serverName) const;
private:
  std::string stashDirectory_;
  std::function<std::string()> cwd_;
  std::function<std::string(const std::string&,const std::string&)> join_;
};

// POSIX host binding for joining path segments.
std::string indigenousPosixJoin(const std::string& left,const std::string& right);
}
