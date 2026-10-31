#include "instructions/project/ProjectDiscovery.hpp"
#include "filesystem/FileOps.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>

namespace batbox::instructions::project {
namespace fs=std::filesystem;
namespace {
std::string slash(std::string path){
  std::replace(path.begin(),path.end(),'\\','/');return path;
}
std::string nodeDirname(std::string path){
  while(path.size()>1&&path.back()=='/')path.pop_back();
  const auto parent=fs::path(path).parent_path().string();
  return parent.empty()?".":parent;
}
}
std::string normalizePathForComparison(const std::string& path,
  const DiscoveryServices& services){
  auto normalized=fs::path(path).lexically_normal().string();
  if(normalized.empty())normalized=".";
  if(services.parse.paths.platform&&services.parse.paths.platform()=="windows"){
    std::replace(normalized.begin(),normalized.end(),'/','\\');
    std::transform(normalized.begin(),normalized.end(),normalized.begin(),
      [](unsigned char c){return static_cast<char>(std::tolower(c));});
  }
  return normalized;
}
std::vector<std::string> resolveExcludePatterns(
  const std::vector<std::string>& patterns){
  std::vector<std::string> expanded;
  expanded.reserve(patterns.size()*2);
  for(const auto& pattern:patterns)expanded.push_back(slash(pattern));
  for(std::size_t i=0;i<expanded.size();++i){
    const auto normalized=expanded[i];
    if(!normalized.starts_with('/'))continue;
    const auto globStart=normalized.find_first_of("*?{[");
    const auto staticPrefix=globStart==std::string::npos
      ?normalized:normalized.substr(0,globStart);
    const auto dirToResolve=nodeDirname(staticPrefix);
    try{
      const auto resolvedDir=slash(fs::canonical(dirToResolve).string());
      if(resolvedDir!=dirToResolve)
        expanded.push_back(resolvedDir+normalized.substr(dirToResolve.size()));
    }catch(...){/* Missing prefix leaves the original pattern in place. */}
  }
  return expanded;
}
bool isBatboxMdExcluded(const std::string& path,const std::string& type,
  const DiscoveryServices& services){
  if(type!="User"&&type!="Project"&&type!="Local")return false;
  if(!services.excludePatterns)
    throw std::invalid_argument("Missing instruction exclude-pattern binding");
  const auto patterns=services.excludePatterns();
  if(patterns.empty())return false;
  auto expanded=resolveExcludePatterns(patterns);
  expanded.erase(std::remove_if(expanded.begin(),expanded.end(),
    [](const auto& pattern){return pattern.empty();}),expanded.end());
  if(expanded.empty())return false;
  if(!services.matchExclude)
    throw std::invalid_argument("Missing picomatch exclusion binding");
  return services.matchExclude(slash(path),expanded);
}
}
