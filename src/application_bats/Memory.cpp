#include "application_bats/Memory.hpp"
#include "git_filesystem/RootFinder.hpp"
#include "instructions/MemoryPoetry.hpp"
#include "memory_prefetch/BatMemoryPath.hpp"
#include "runtime/IndigenousPath.hpp"
#ifdef _WIN32
#include "runtime/IndigenousUnicode.hpp"
#endif
#include <cstdlib>
#include <filesystem>
#include <vector>

namespace batbox::application_bats {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::optional<std::string> environment(const char* name){
#ifdef _WIN32
  return runtime::environmentUtf8Optional(name);
#else
  const auto* value=std::getenv(name);
  return value?std::optional<std::string>(value):std::nullopt;
#endif
}
instructions::MemoryPathServices memoryPaths(const application::Config& config,
    const Json& state,const std::filesystem::path& root){
  instructions::MemoryPathServices paths;
  paths.configHomeDir=runtime::pathToUtf8(config.stateDirectory);
  paths.projectRoot=runtime::pathToUtf8(root.empty()?config.cwd:root);
  paths.initialSettings=field(state,"settings");
  if(!paths.initialSettings.is_object())paths.initialSettings=Json::object();
#ifdef _WIN32
  if(const auto home=environment("USERPROFILE"))paths.homeDirectory=*home;
  else if(const auto home=environment("HOME"))paths.homeDirectory=*home;
#else
  if(const auto home=environment("HOME"))paths.homeDirectory=*home;
  else if(const auto home=environment("USERPROFILE"))paths.homeDirectory=*home;
#endif
  for(const auto& key:{paths.keys.disableAutoMemory,paths.keys.simpleMode,
      paths.keys.remoteMode,paths.keys.remoteMemoryDir,
      paths.keys.fullPathOverride})
    if(const auto value=environment(key.c_str()))paths.environment[key]=*value;
  auto finder=git_filesystem::indigenousRootFinder();
  paths.findCanonicalGitRoot=[finder](const std::string& current){
    return finder->find(current);};
  return paths;
}
std::string scopeNote(const std::string& scope){
  if(scope=="user")return "- Since this memory is user-scope, keep learnings general since they apply across all projects";
  if(scope=="project")return "- Since this memory is project-scope and shared with your team via version control, tailor your memories to this project";
  return "- Since this memory is local-scope (not checked into version control), tailor your memories to this project and machine";
}
}
bool batMemoryEnabled(const application::Config& config,
    const Json& appState,const std::filesystem::path& projectRoot){
  return instructions::isAutoMemoryEnabled(memoryPaths(config,appState,
    projectRoot));
}
std::string loadBatMemoryPoetry(const Json& definition,
    const application::Config& config,const Json& appState,
    const std::filesystem::path& projectRoot,
    const std::function<void(const std::string&)>& diagnostic){
  const auto& rawScope=field(definition,"memory");
  if(!rawScope.is_string())return {};
  const auto scope=rawScope.get<std::string>();
  if(scope!="user"&&scope!="project"&&scope!="local")return {};
  auto paths=memoryPaths(config,appState,projectRoot);
  if(!instructions::isAutoMemoryEnabled(paths))return {};
  const auto& rawName=field(definition,"agentType");
  if(!rawName.is_string())return {};
  const auto name=rawName.get<std::string>();
  const auto directory=memory_prefetch::getBatMemoryDir(name,scope,
    runtime::pathToUtf8(config.cwd),paths);
  instructions::MemoryPoetryServices services;
  services.paths=std::move(paths);
  services.originalCwd=runtime::pathToUtf8(config.cwd);
  services.logDebug=diagnostic;
  instructions::ensureMemoryDirExists(directory,services);
  std::vector<std::string> guidelines={scopeNote(scope)};
  if(const auto extra=environment("BATBOX_COWORK_MEMORY_EXTRA_GUIDELINES"))
    if(!value::trim(*extra).empty())guidelines.emplace_back(*extra);
  return instructions::buildMemoryPoetry("Persistent Agent Memory",
    directory,guidelines,services);
}
std::string batSystemPoetryText(const Json& definition,
    const std::string& fallback,const application::Config& config,
    const Json& appState,const std::filesystem::path& projectRoot,
    const std::function<void(const std::string&)>& diagnostic){
  const auto& selected=field(definition,"systemPrompt");
  std::string poetry=selected.is_string()?selected.get<std::string>():fallback;
  const auto memory=loadBatMemoryPoetry(definition,config,appState,
    projectRoot,diagnostic);
  if(!memory.empty())poetry+="\n\n"+memory;
  return poetry;
}
}
