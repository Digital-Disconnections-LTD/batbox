#include "attachment_triggers/Attachments.hpp"
#include <chrono>
#include <filesystem>
#include <stdexcept>

namespace batbox::attachment_triggers {
namespace {
double nowMillis(const Services& services){
  if(services.nowMillis)return services.nowMillis();
  return std::chrono::duration<double,std::milli>(
    std::chrono::system_clock::now().time_since_epoch()).count();
}
std::string displayPath(const std::string& path,const std::string& cwd){
  const auto relative=std::filesystem::path(path).lexically_relative(cwd);
  return relative.empty()?path:relative.generic_string();
}
bool hookMemoryType(const std::string& type){
  return type=="User"||type=="Project"||type=="Local"||type=="Managed";
}
}
Json memoryFilesToAttachments(const MemoryFiles& files,execution::Context& context,
  const Services& services,const std::optional<std::string>& triggerFilePath){
  if(!context.readFileState)
    throw std::invalid_argument("Nested memory requires typed file cache");
  const bool fireHook=instructions::hooks::hasInstructionsLoadedHook(services.hooks);
  Json attachments=Json::array();
  for(const auto& file:files){
    if(context.loadedNestedMemoryPaths&&
       context.loadedNestedMemoryPaths->contains(file.path))continue;
    if(context.readFileState->has(file.path))continue;
    attachments.push_back(Json{{"type","nested_memory"},{"path",file.path},
      {"content",instructions::project::memoryFileToJson(file)},
      {"displayPath",displayPath(file.path,services.cwd)}});
    if(context.loadedNestedMemoryPaths)
      context.loadedNestedMemoryPaths->insert(file.path);
    context.readFileState->set(file.path,file_state_stash::FileState{
      file.contentDiffersFromDisk?file.rawContent.value_or(file.content):file.content,
      nowMillis(services),file.contentDiffersFromDisk,{},{},true});
    if(fireHook&&hookMemoryType(file.type)){
      const std::string reason=file.globs?"path_glob_match":
        file.parent?"include":"nested_traversal";
      instructions::hooks::InstructionsLoadedOptions options;
      options.globs=file.globs;
      options.triggerFilePath=triggerFilePath;
      options.parentFilePath=file.parent;
      (void)instructions::hooks::executeInstructionsLoadedHooks(services.loop,
        services.hooks,file.path,file.type,reason,options);
    }
  }
  return attachments;
}
} // namespace batbox::attachment_triggers
