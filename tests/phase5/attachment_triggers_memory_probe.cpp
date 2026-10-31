#include "attachment_triggers/Attachments.hpp"
#include <iostream>

using namespace batbox;
namespace {
instructions::project::MemoryFileInfo fileFromJson(const Json& value){
  instructions::project::MemoryFileInfo file;
  file.path=value.at("path").get<std::string>();
  file.type=value.at("type").get<std::string>();
  file.content=value.at("content").get<std::string>();
  if(value.contains("parent"))file.parent=value.at("parent").get<std::string>();
  if(value.contains("globs"))file.globs=value.at("globs").get<std::vector<std::string>>();
  file.contentDiffersFromDisk=value.value("contentDiffersFromDisk",false);
  if(value.contains("rawContent"))file.rawContent=value.at("rawContent").get<std::string>();
  return file;
}
Json run(const Json& input){
  const auto cwd=input.value("cwd",std::string("/work"));
  if(input.value("mode",std::string())=="directories"){
    const auto dirs=attachment_triggers::getDirectoriesToProcess(
      input.at("target").get<std::string>(),
      input.at("originalCwd").get<std::string>(),cwd);
    return {{"nestedDirs",dirs.nestedDirs},{"cwdLevelDirs",dirs.cwdLevelDirs}};
  }
  event_loop::Loop loop;
  attachment_triggers::Services services{loop};
  services.cwd=cwd;
  services.nowMillis=[](){return 1700000000000.0;};
  const bool hook=input.value("hook",false);
  services.hooks.getHooksConfigFromSnapshot=[hook]()->std::optional<Json>{
    if(hook)return Json{{"InstructionsLoaded",Json::array({Json::object()})}};
    return std::nullopt;
  };
  services.hooks.getRegisteredHooks=[]()->std::optional<Json>{return std::nullopt;};
  services.hooks.createBaseHookInput=[](){return Json::object();};
  services.hooks.defaultTimeoutMs=1000;
  Json trace=Json::array();
  services.hooks.executeHooksOutsideRepl=[&](instructions::hooks::HookDispatch dispatch){
    Json options=Json::object();
    if(dispatch.hookInput.contains("globs"))options["globs"]=dispatch.hookInput["globs"];
    if(dispatch.hookInput.contains("trigger_file_path"))
      options["triggerFilePath"]=dispatch.hookInput["trigger_file_path"];
    if(dispatch.hookInput.contains("parent_file_path"))
      options["parentFilePath"]=dispatch.hookInput["parent_file_path"];
    trace.push_back(Json{{"file",dispatch.hookInput["file_path"]},
      {"type",dispatch.hookInput["memory_type"]},
      {"reason",dispatch.hookInput["load_reason"]},{"options",options}});
    event_loop::Deferred<Json> result(loop);result.resolve(Json::object());return result;
  };
  execution::Context context;
  context.readFileState=file_state_stash::createFileStateStashWithSizeLimit(100);
  context.loadedNestedMemoryPaths=std::make_shared<runtime::OrderedStringSet>();
  if(input.contains("loaded"))for(const auto& name:input.at("loaded"))
    context.loadedNestedMemoryPaths->insert(name.get<std::string>());
  if(input.contains("cache"))for(auto it=input.at("cache").begin();it!=input.at("cache").end();++it)
    context.readFileState->set(it.key(),file_state_stash::fromJson(it.value()));
  attachment_triggers::MemoryFiles files;
  for(const auto& value:input.at("files"))files.push_back(fileFromJson(value));
  std::optional<std::string> trigger;
  if(input.contains("triggerFilePath"))trigger=input.at("triggerFilePath").get<std::string>();
  const auto attachments=attachment_triggers::memoryFilesToAttachments(files,
    context,services,trigger);
  Json loaded=Json::array();
  for(const auto& name:*context.loadedNestedMemoryPaths)loaded.push_back(name);
  Json states=Json::object();
  for(const auto& file:files)if(const auto value=context.readFileState->get(file.path))
    states[file.path]=file_state_stash::toJson(*value);
  return {{"attachments",attachments},{"loaded",loaded},{"states",states},
    {"trace",trace}};
}
}
int main(){std::string line;while(std::getline(std::cin,line)){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify(Json{{"error",error.what()}})<<'\n';}
}}
