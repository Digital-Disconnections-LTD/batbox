#include "stash_paths/StashPaths.hpp"
#include "runtime/PathSanitization.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::stash_paths {
std::string sanitizePath(const std::string& name){
  // Use the shared path sanitizer for consistent stash directory names.
  return runtime::sanitizePath(name);
}

std::string getProjectDir(const std::string& cwd){return sanitizePath(cwd);}

StashPaths::StashPaths(Imports imports):
    stashDirectory_(imports.stashDirectory?imports.stashDirectory():""),
    cwd_(std::move(imports.cwd)),join_(std::move(imports.join)){
  if(!imports.stashDirectory||!cwd_||!join_)
    throw std::invalid_argument("Cache path imports are required");
}

std::string StashPaths::baseLogs() const {
  return join_(stashDirectory_,getProjectDir(cwd_()));
}
std::string StashPaths::errors() const {
  return join_(baseLogs(),"errors");
}
std::string StashPaths::messages() const {
  return join_(baseLogs(),"messages");
}
std::string StashPaths::mcpLogs(const std::string& serverName) const {
  return join_(baseLogs(),"mcp-logs-"+sanitizePath(serverName));
}
}
