#pragma once
#include "git_filesystem/GitFilesystem.hpp"

namespace batbox::git_filesystem {
std::optional<std::string> parseConfigString(const std::string& config,
  const std::string& section,const std::optional<std::string>& subsection,
  const std::string& key);
Deferred<std::optional<std::string>> parseGitConfigValue(
  event_loop::Loop& loop,const Services& services,const std::string& gitDir,
  const std::string& section,const std::optional<std::string>& subsection,
  const std::string& key);
void bindIndigenousConfigParser(event_loop::Loop& loop,Services& services);
} // namespace batbox::git_filesystem
