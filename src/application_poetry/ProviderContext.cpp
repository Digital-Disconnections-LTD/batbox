#include "application_poetry/Provider.hpp"
#include "glob/DiscoveryMatcherBinding.hpp"
#include "git/GitBindings.hpp"
#include "instructions/project/async/ContextBinding.hpp"
#include "runtime/Unicode.hpp"
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace batbox::application_poetry {
namespace {
std::vector<std::string> stringArray(const Json& value){
  std::vector<std::string> result;
  if(value.is_array())for(const auto& item:value)
    if(item.is_string())result.push_back(item.get<std::string>());
  return result;
}
bool inPath(const std::string& child,const std::string& base){
  const auto relative=std::filesystem::path(child).lexically_normal().
    lexically_relative(std::filesystem::path(base).lexically_normal());
  if(relative.empty())return false;
  for(const auto& component:relative)
    if(component=="..")return false;
  return true;
}
}
void Provider::bindDiscovery(){
  const auto home=std::getenv("HOME");
  memoryPaths_.homeDirectory=home?home:"";
  memoryPaths_.configHomeDir=inputs_.config.stateDirectory.string();
  memoryPaths_.projectRoot=cwd();
  memoryPaths_.environment=inputs_.environment;
  memoryPaths_.initialSettings=settings();
  memoryPaths_.settingsBySource["userSettings"]=memoryPaths_.initialSettings;
  memoryPaths_.findCanonicalGitRoot=[this](const std::string& path){
    return canonicalRoot_->find(path);
  };
  const auto* self=this;
  discovery_.parse.paths.getCwd=[self]{return self->cwd();};
  discovery_.parse.paths.homeDirectory=[self]{
    return self->memoryPaths_.homeDirectory;};
  discovery_.parse.paths.platform=[] {
#if defined(_WIN32)
    return std::string("windows");
#elif defined(__APPLE__)
    return std::string("darwin");
#else
    return std::string("linux");
#endif
  };
  discovery_.parse.paths.normalizeNFC=value::normalizeNFC;
  discovery_.parse.logDebug=[this](const std::string& message){
    if(inputs_.logDiagnostic)
      inputs_.logDiagnostic("debug",message,std::nullopt);
  };
  discovery_.excludePatterns=[this]{
    return stringArray(value::property(settings(),"memoryExcludePatterns"));
  };
  glob::bindDiscoveryMatchers(discovery_);
  discovery_.pathInWorkingPath=inPath;
  discovery_.originalCwd=[this]{return cwd();};
  discovery_.memoryPath=[this](const std::string& type){
    if(type=="Managed")return (inputs_.config.stateDirectory/
      "instructions/managed/BATBOX.md").string();
    if(type=="User")return (application::configurationDirectory(inputs_.config)/
      "BATBOX.md").string();
    throw std::invalid_argument("Unknown instruction source: "+type);
  };
  discovery_.managedRulesDir=[this]{return (inputs_.config.stateDirectory/
    "instructions/managed/rules").string();};
  discovery_.userRulesDir=[this]{return (application::configurationDirectory(
    inputs_.config)/"rules").string();};
  discovery_.settingSourceEnabled=[this](const std::string& source){
    const auto current=settings();
    const auto& configured=value::property(
      value::property(current,"settingSources"),source);
    return configured.is_null()||value::truthy(configured);
  };
  discovery_.externalIncludesApproved=[this]{
    return value::truthy(value::property(settings(),"allowExternalInstructions"));
  };
  discovery_.additionalDirectoriesEnabled=[this]{
    return !inputs_.additionalDirectories.empty();
  };
  discovery_.additionalDirectories=[this]{return inputs_.additionalDirectories;};
  discovery_.findGitRoot=[this](const std::string& path){
    return rootFinder_->find(path);
  };
  discovery_.findCanonicalGitRoot=[this](const std::string& path){
    return canonicalRoot_->find(path);
  };
  discovery_.autoMemoryEnabled=[this]{
    memoryPaths_=memoryPathServices();
    return instructions::isAutoMemoryEnabled(memoryPaths_);
  };
  discovery_.autoMemoryEntrypoint=[this]{
    return instructions::getAutoMemEntrypoint(memoryPaths_,memoryPathStash_);
  };
  discovery_.teamMemoryEnabled=[] {return false;};
  discovery_.teamMemoryEntrypoint=[] {return std::string{};};
  discovery_.hasInstructionsLoadedHook=[this]{
    return bool(inputs_.instructionsLoadedHook);
  };
  discovery_.instructionsLoadedHook=[this](const auto& file,
      const auto& reason){
    if(inputs_.instructionsLoadedHook)
      inputs_.instructionsLoadedHook(file,reason);
  };
  discovery_.skipAutoMemoryIndex=[this]{
    return value::truthy(value::property(settings(),"skipAutoMemoryIndex"));
  };
  discovery_.skipProjectInstructions=[this]{
    return value::truthy(value::property(settings(),"skipProjectInstructions"));
  };
  discovery_.teamMemoryFeature=[] {return false;};
  discovery_.externalIncludesWarningShown=[] {return false;};
  discoveryAsync_=instructions::project::async::indigenousAsyncFs(owner_,discovery_);
}
void Provider::bindContext(){
  contextServices_.environment=inputs_.environment;
  contextServices_.testEnvironment=inputs_.testEnvironment;
  contextServices_.logError=[this](std::exception_ptr error){
    if(inputs_.logError)inputs_.logError(error);
  };
  contextServices_.includeGitInstructions=[this]{
    return context::shouldIncludeGitInstructions(inputs_.environment,settings());
  };
  contextServices_.stashBreakerFeature=[] {return false;};
  contextServices_.bareMode=[this]{
    return value::truthy(value::property(settings(),"bareMode"));
  };
  contextServices_.additionalInstructionDirectories=[this]{
    return inputs_.additionalDirectories;
  };
  contextServices_.projectServices=&discovery_;
  contextServices_.setStashedBatboxMdContent=[this](const auto& content){
    stashedBatboxMdContent_=content;
  };
  contextServices_.localISODate=[] {return context::getLocalISODate();};
  git::bindGitContextServices(contextServices_,*gitBasics_);
  contextServices_.gitStdoutNoThrow=[this](const std::vector<std::string>& args){
    event_loop::Deferred<std::string> output(owner_);
    process::ExecOptions options;
    options.preserveOutputOnError=false;
    options.cwd=cwd();
    process::execFileNoThrow(owner_,gitBasics_->gitExe(),args,
      options,execImports_).then([output](const process::ExecResult* value,
        const std::exception_ptr& error){
      if(error)output.reject(error);else output.resolve(value->stdoutText);
    });
    return output;
  };
  instructions::project::async::bindMemoryContextServices(
    contextServices_,discoveryStash_,discoveryAsync_);
}
} // namespace batbox::application_poetry
