#include "settings/persistence/WatchTargets.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <filesystem>

namespace batbox::settings::persistence {
namespace {
bool contains(const std::vector<std::string>& values,
    const std::string& value){
  return std::find(values.begin(),values.end(),value)!=values.end();
}
std::string normalize(const std::string& value){
  return runtime::pathToUtf8(runtime::pathFromUtf8(value).lexically_normal());
}
}
bool ignoreWatchPath(const WatchTargets& targets,const std::string& path,
    const std::optional<WatchStat>& stat){
  if(stat&&!stat->file&&!stat->directory)return true;
  if(!targets.includeGitPaths)
    for(const auto& component:runtime::pathFromUtf8(path))
      if(component==".git")return true;
  if(!stat||stat->directory)return false;
  const auto normalized=normalize(path);
  return !contains(targets.settingsFiles,normalized);
}
}
