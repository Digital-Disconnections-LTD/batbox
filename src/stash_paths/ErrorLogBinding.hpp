#pragma once
#include "stash_paths/StashPaths.hpp"
#include <memory>
#include <stdexcept>
#include <utility>

namespace batbox::stash_paths {
struct ErrorLogPathBindings {
  std::function<std::string()> errorsDirectory;
  std::function<std::string(const std::string&)> mcpLogsDirectory;
};

inline ErrorLogPathBindings bindErrorLogPaths(std::shared_ptr<StashPaths> paths){
  if(!paths)throw std::invalid_argument("Cache paths are required");
  return {
    [paths]{return paths->errors();},
    [paths](const std::string& server){return paths->mcpLogs(server);}
  };
}
}
