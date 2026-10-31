#include "settings/persistence/WatchTargets.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <filesystem>
#include <map>
#include <stdexcept>

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
WatchPathServices bindWatchPaths(Paths& paths){
  return {
    [&paths](Source source){return paths.getSettingsFilePathForSource(source);},
    [&paths](){return paths.getManagedSettingsDropInDir();}
  };
}
WatchStatReader indigenousWatchStatReader(){
  return [](const std::string& path)->std::optional<WatchStat>{
    std::error_code error;
    const auto status=std::filesystem::status(runtime::pathFromUtf8(path),error);
    if(error)return std::nullopt;
    return WatchStat{std::filesystem::is_regular_file(status),
      std::filesystem::is_directory(status)};
  };
}
WatchTargets getWatchTargets(const WatchPathServices& paths,
    const WatchStatReader& stat){
  if(!stat)throw std::invalid_argument("Missing settings watch stat reader");
  if(!paths.getPathForSource||!paths.getManagedDropInDir)
    throw std::invalid_argument("Missing settings watch path binding");
  std::map<std::string,std::vector<std::string>> dirToFiles;
  std::vector<std::string> dirsWithExistingFiles;
  for(const auto source:settingSources()){
    if(source==Source::Flag)continue;
    const auto path=paths.getPathForSource(source);
    if(!path||path->empty())continue;
    const auto parent=runtime::pathToUtf8(
      runtime::pathFromUtf8(*path).parent_path());
    const auto dir=parent.empty()?std::string("."):parent;
    auto& files=dirToFiles[dir];
    if(!contains(files,*path))files.push_back(*path);
    const auto info=stat(*path);
    if(info&&info->file&&!contains(dirsWithExistingFiles,dir))
      dirsWithExistingFiles.push_back(dir);
  }
  WatchTargets result;
  for(const auto& dir:dirsWithExistingFiles)
    for(const auto& file:dirToFiles.at(dir))
      if(!contains(result.settingsFiles,file))result.settingsFiles.push_back(file);
  const auto managedDropIn=paths.getManagedDropInDir();
  const auto dropInfo=stat(managedDropIn);
  if(dropInfo&&dropInfo->directory){
    if(!contains(dirsWithExistingFiles,managedDropIn))
      dirsWithExistingFiles.push_back(managedDropIn);
    result.dropInDir=managedDropIn;
  }
  result.dirs=std::move(dirsWithExistingFiles);
  return result;
}
std::optional<Source> getSourceForPath(const WatchPathServices& paths,
    const std::string& path){
  if(!paths.getPathForSource||!paths.getManagedDropInDir)
    throw std::invalid_argument("Missing settings watch path binding");
  const auto normalized=normalize(path);
  const auto dropIn=paths.getManagedDropInDir();
  if(normalized.starts_with(dropIn+std::string(1,
      std::filesystem::path::preferred_separator)))return Source::Policy;
  for(const auto source:settingSources())
    if(paths.getPathForSource(source)==normalized)return source;
  return std::nullopt;
}
std::string settingSourceToConfigChangeSource(Source source){
  switch(source){
    case Source::User:return "user_settings";
    case Source::Project:return "project_settings";
    case Source::Local:return "local_settings";
    case Source::Flag:
    case Source::Policy:return "policy_settings";
  }
  throw std::invalid_argument("Unknown settings source");
}
bool ignoreWatchPath(const WatchTargets& targets,const std::string& path,
    const std::optional<WatchStat>& stat){
  if(stat&&!stat->file&&!stat->directory)return true;
  if(!targets.includeGitPaths)
    for(const auto& component:runtime::pathFromUtf8(path))
      if(component==".git")return true;
  if(!stat||stat->directory)return false;
  const auto normalized=normalize(path);
  if(contains(targets.settingsFiles,normalized))return false;
  if(targets.dropInDir&&
      normalized.starts_with(*targets.dropInDir+std::string(1,
        std::filesystem::path::preferred_separator))&&
      normalized.ends_with(".json"))return false;
  return true;
}
}
