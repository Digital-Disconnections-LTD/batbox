#include "instructions/project/ProjectDiscovery.hpp"
#include "application_poetry/Provider.hpp"
#include "application_poetry/Stamp.hpp"
#include "cwd/Override.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/Platform.hpp"
#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace batbox::application_poetry {
namespace {
std::string memoryStamp(const std::string& file){
  return fileStamp(runtime::pathFromUtf8(file));
}
Json environmentSnapshot(){
  Json result=Json::object();
  for(const char* key:{"SHELL","BATBOX_SIMPLE",
      "BATBOX_DISABLE_BATBOX_MDS","BATBOX_OVERRIDE_DATE",
      "BATBOX_DISABLE_GIT_INSTRUCTIONS","BATBOX_DISABLE_AUTO_MEMORY",
      "BATBOX_REMOTE_MEMORY_DIR","BATBOX_MEMORY_PATH_OVERRIDE"})
    if(const auto* value=std::getenv(key))result[key]=value;
  return result;
}
}
Provider::Provider(event_loop::Loop& owner,Inputs inputs):owner_(owner),
    inputs_(std::move(inputs)),poetrySections_(owner),contextStash_(owner),
    discoveryStash_(owner),systemPoetryFile_(application::configurationDirectory(inputs_.config)){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Prompt provider requires owner loop");
  if(inputs_.config.cwd.empty()||inputs_.config.stateDirectory.empty())
    throw std::invalid_argument("Prompt cwd and native state directory required");
  if(!inputs_.environment.is_object())
    throw std::invalid_argument("Prompt environment must be an object");
  const auto captured=environmentSnapshot();
  for(auto it=captured.begin();it!=captured.end();++it)
    if(!inputs_.environment.contains(it.key()))
      inputs_.environment[it.key()]=it.value();
  bindGit();bindDiscovery();bindContext();
}
Json Provider::settings()const{
  const auto value=inputs_.initialSettings
    ?inputs_.initialSettings():Json::object();
  if(!value.is_object())
    throw std::invalid_argument("Initial settings must be an object");
  return value;
}
std::string Provider::cwd()const{
  return cwd::pwd([this]{return runtime::pathToUtf8(inputs_.config.cwd);});
}
event_loop::Deferred<Json> Provider::systemContext(){
  return contextStash_.getSystemContext(contextServices_);
}
event_loop::Deferred<Json> Provider::userContext(){
  refreshMemoryFilesIfChanged();
  event_loop::Deferred<Json> output(owner_);
  struct Parts {std::optional<Json> context;std::optional<std::string> recovery;};
  auto parts=std::make_shared<Parts>();
  const auto finish=[parts,output]{
    if(!parts->context||!parts->recovery)return;
    auto result=std::move(*parts->context);
    if(!parts->recovery->empty())result["projectRecovery"]=*parts->recovery;
    output.resolve(std::move(result));
  };
  contextStash_.getUserContext(contextServices_).then(
    [parts,finish,output](const Json* value,const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      parts->context=*value;finish();
    });
  projectRecovery().then([parts,finish,output](const std::string* value,
      const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    parts->recovery=*value;finish();
  });
  return output;
}
event_loop::Deferred<context::MemoryFiles> Provider::memoryFiles(){
  refreshMemoryFilesIfChanged();
  event_loop::Deferred<context::MemoryFiles> output(owner_);
  auto* self=this;
  discoveryStash_.getMemoryFiles(discoveryAsync_).then([self,output](
      const context::MemoryFiles* files,const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    try{
      for(const auto& file:*files){
        if(std::find(self->observedInstructionFiles_.begin(),
            self->observedInstructionFiles_.end(),file.path)!=
            self->observedInstructionFiles_.end())continue;
        self->observedInstructionFiles_.push_back(file.path);
        if(!instructions::project::getLargeMemoryFiles({file}).empty()&&
            self->inputs_.logDiagnostic)
          self->inputs_.logDiagnostic("instructions",file.path+" is "+
            std::to_string(value::utf16CodeUnits(file.content).size())+
            " characters; it is sent whole with every request",std::nullopt);
        self->memoryFileStamps_[file.path]=memoryStamp(file.path);
        if(self->stablePromptPrefix())self->trackHeadFile(file.path);
      }
      output.resolve(*files);
    }catch(...){output.reject(std::current_exception());}
  });
  return output;
}
instructions::MemoryPathServices Provider::memoryPathServices(){
  if(!owner_.isOwnerThread())throw std::logic_error("Memory paths require owner loop");
  auto result=memoryPaths_;
  result.initialSettings=settings();
  const auto& bySource=value::property(result.initialSettings,"settingsBySource");
  if(bySource.is_object())result.settingsBySource=bySource;
  else result.settingsBySource["userSettings"]=result.initialSettings;
  return result;
}
void Provider::refreshMemoryFilesIfChanged(){
  if(!owner_.isOwnerThread())throw std::logic_error("Memory refresh requires owner loop");
  const bool stable=stablePromptPrefix();
  // A stable head is tracked once per snapshot; later instruction files are
  // tracked as memoryFiles() discovers them.
  if(stable&&!headFiles_.empty())return;
  auto current=memoryPathServices();
  instructions::MemoryPathStash freshPaths;
  std::vector<std::string> files={instructions::getAutoMemEntrypoint(current,freshPaths),
    runtime::pathToUtf8(application::configurationDirectory(inputs_.config)/"BATBOX.md"),
    runtime::pathToUtf8(inputs_.config.cwd/"BATBOX.md"),
    runtime::pathToUtf8(inputs_.config.cwd/"BATBOX.local.md")};
  files.insert(files.end(),observedInstructionFiles_.begin(),observedInstructionFiles_.end());
  if(stable){
    // Changes reach the model at the tail (takeHeadChangeAttachments); the
    // head keeps the bytes it was built with.
    memoryPaths_=std::move(current);
    for(const auto& file:files)trackHeadFile(file);
    return;
  }
  std::map<std::string,std::string> stamps;
  for(const auto& file:files)stamps[file]=memoryStamp(file);
  if(stamps!=memoryFileStamps_){
    const bool first=memoryFileStamps_.empty();
    memoryFileStamps_=std::move(stamps);
    memoryPaths_=std::move(current);
    if(!first){clearProjectInstructions();clearPoetrySections();}
  }
}
const poetryEntries::SystemPoetrySource& Provider::systemPoetrySource(){
  if(!owner_.isOwnerThread())throw std::logic_error("System prompt file requires owner loop");
  if(systemHeadFrozen_&&stablePromptPrefix())return systemPoetryFile_.source();
  systemHeadFrozen_=true;
  if(!systemPoetryFile_.refresh())return systemPoetryFile_.source();
  const auto& source=systemPoetryFile_.source();
  if(inputs_.logDiagnostic){
    if(source.fromFile)inputs_.logDiagnostic("system poetry","using "+
      poetryEntries::describeSystemPoetry(source),std::nullopt);
    for(const auto& warning:poetryEntries::systemPoetryWarnings(source))
      inputs_.logDiagnostic("system poetry",warning+" ("+(source.fromFile?
        poetryEntries::displayPath(source.path):std::string("built-in"))+")",std::nullopt);
  }
  return source;
}
void Provider::clearPoetrySections(){
  systemHeadFrozen_=false;
  poetrySections_.clearSystemPoetrySections([this]{
    if(inputs_.clearBetaHeaderLatches)inputs_.clearBetaHeaderLatches();
  });
}
void Provider::clearProjectInstructions(){
  resetHeadSnapshot();
  projectRecoveryStash_.reset();
  discoveryStash_.clearMemoryFileStashes();
  memoryPathStash_.byProjectRoot.clear();
  contextStash_.clearUserContextStash();
}
void Provider::resetProjectInstructionsAfterCompact(){
  resetHeadSnapshot();
  projectRecoveryStash_.reset();
  discoveryStash_.resetGetMemoryFilesStash("compact");
  contextStash_.clearUserContextStash();
}
void Provider::clearGitStatus(){
  contextStash_.clearGitStatusStash();
  contextStash_.clearSystemContextStash();
}
} // namespace batbox::application_poetry
