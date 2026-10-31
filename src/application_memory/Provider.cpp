#include "application_memory/Provider.hpp"
#include "memory_prefetch/BatMemoryPath.hpp"
#include "runtime/IndigenousPath.hpp"
#include "utils/envUtils.hpp"
#include <filesystem>
#include <stdexcept>

namespace batbox::application_memory {
std::shared_ptr<Provider> Provider::create(Inputs inputs){
  if(!inputs.owner.isOwnerThread()||!inputs.poetry||!inputs.model||
      !inputs.flight||!inputs.messages||!inputs.getAppState)
    throw std::invalid_argument("Memory needs native prompt, model, session and state owners");
  if(!inputs.log)inputs.log=[](const std::string&){};
  return std::shared_ptr<Provider>(new Provider(std::move(inputs)));
}
void Provider::requireOwner()const{
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Memory requires owner loop");
}
bool Provider::hasActiveJobs()const{
  requireOwner();return recallJobs_!=0||activeDream_.has_value();
}
Json Provider::settings()const{
  const auto state=inputs_.getAppState();
  const auto& value=value::property(state,"settings");
  return value.is_object()?value:Json::object();
}
instructions::MemoryPathServices Provider::paths(){
  requireOwner();return inputs_.poetry->memoryPathServices();
}
std::string Provider::autoMemoryDirectory(){
  requireOwner();return instructions::getAutoMemPath(paths(),pathStash_);
}
bool Provider::recallEnabled()const{
  const auto current=settings();
  const auto& configured=value::property(current,"memoryRecallEnabled");
  // Recall is enabled by default and can be disabled in local settings.
  return configured.is_null()||value::truthy(configured);
}
bool Provider::dreamEnabled()const{
  return value::property(settings(),"autoDreamEnabled")==true;
}
std::vector<std::pair<std::string,std::string>> Provider::memoryDirectories(){
  requireOwner();
  auto services=paths();
  std::vector<std::pair<std::string,std::string>> dirs;
  dirs.emplace_back(autoMemoryDirectory(),"project");
  const auto state=inputs_.getAppState();
  const auto& bats=value::property(value::property(state,"agentDefinitions"),"activeAgents");
  if(bats.is_array())for(const auto& bat:bats){
    const auto& name=value::property(bat,"agentType");
    const auto& scope=value::property(bat,"memory");
    if(!name.is_string()||!scope.is_string())continue;
    if(scope!="user"&&scope!="project"&&scope!="local")continue;
    dirs.emplace_back(memory_prefetch::getBatMemoryDir(name.get<std::string>(),
      scope.get<std::string>(),runtime::pathToUtf8(inputs_.config.cwd),services),
      scope.get<std::string>());
  }
  return dirs;
}
void Provider::refresh(){
  requireOwner();pathStash_.byProjectRoot.clear();
  inputs_.poetry->clearProjectInstructions();
  inputs_.poetry->clearPoetrySections();
  if(prefetch_)prefetch_->paths=paths();
}
void Provider::observeFileChange(const std::string& file){
  requireOwner();
  if(file.empty()){inputs_.poetry->refreshMemoryFilesIfChanged();return;}
  auto path=runtime::pathFromUtf8(file);
  if(path.is_relative())path=inputs_.config.cwd/path;
  path=path.lexically_normal();
  const auto name=path.filename();
  if(name=="BATBOX.md"||name=="BATBOX.local.md"||path.extension()==".md"){
    for(const auto& [directory,scope]:memoryDirectories()){
      (void)scope;
      const auto relative=path.lexically_relative(runtime::pathFromUtf8(directory));
      if(!relative.empty()&&!relative.is_absolute()&&*relative.begin()!=".."){
        refresh();return;
      }
    }
    if(name=="BATBOX.md"||name=="BATBOX.local.md")refresh();
  }
}
} // namespace batbox::application_memory
