#include "application_skills/Provider.hpp"
#include "application/Async.hpp"
#include "application_hooks/Provider.hpp"
#include "application_media/AttachmentAggregate.hpp"
#include "application_flights/Flight.hpp"
#include "git_filesystem/RootFinder.hpp"
#include "runtime/IndigenousPath.hpp"
#ifdef _WIN32
#include "runtime/IndigenousUnicode.hpp"
#endif
#include "runtime/Platform.hpp"
#include "shell/ShellQuote.hpp"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace batbox::application_skills {
namespace {
std::string home(){
#ifdef _WIN32
  const auto profile=runtime::environmentUtf8Optional("USERPROFILE");
  if(profile)return *profile;
  return runtime::environmentUtf8("HOME");
#else
  const char* value=std::getenv("HOME");
  if(!value)value=std::getenv("USERPROFILE");
  return value?value:"";
#endif
}
void owner(const event_loop::Loop& loop){
  if(!loop.isOwnerThread())throw std::logic_error("Skill registry needs owner loop");
}
bool containsPoetryShell(const std::string& text){
  return text.find("```!")!=std::string::npos||text.find("!`")!=std::string::npos;
}
Json strings(const std::vector<std::string>& values){
  Json result=Json::array();for(const auto& value:values)result.push_back(value);
  return result;
}
}
std::shared_ptr<Provider> Provider::create(Inputs inputs){
  if(!inputs.owner.isOwnerThread()||!inputs.messages||!inputs.flight||
      !inputs.model||!inputs.invoked||!inputs.currentMessages||
      !inputs.currentBat||!inputs.appState)
    throw std::invalid_argument("Skill provider requires live application owners");
  auto result=std::shared_ptr<Provider>(new Provider(std::move(inputs)));
  result->initialize();return result;
}
void Provider::initialize(){
  roster_=std::make_shared<skills::SkillRoster>();
  services_.homeDir=home();
  services_.configHomeDir=runtime::pathToUtf8(
    application::configurationDirectory(inputs_.config));
#ifdef _WIN32
  const auto programData=runtime::environmentUtf8Optional("PROGRAMDATA");
  services_.managedPath=programData?*programData+"/Batbox":"";
#else
  services_.managedPath="/etc/batbox";
#endif
  const auto finder=git_filesystem::indigenousRootFinder();
  const auto root=finder->find(runtime::pathToUtf8(inputs_.config.cwd));
  services_.gitRoot=root.value_or("");
  if(root&&!inputs_.projectRoot.empty()&&
      runtime::pathFromUtf8(*root).lexically_normal()!=
        inputs_.projectRoot.lexically_normal()&&
      !std::filesystem::is_directory(runtime::pathFromUtf8(*root)/
        ".batbox"/"skills"))
    services_.additionalDirs.push_back(runtime::pathToUtf8(inputs_.projectRoot));
  auto weak=weak_from_this();
  services_.loadLegacyCommands=[weak](const std::string& cwd){
    if(auto self=weak.lock())return self->loadLegacy(cwd);
    return std::vector<skills::SkillWithPath>{};};
  services_.skillsLoaded=[weak]{if(auto self=weak.lock()){
    if(self->inputs_.changed)self->inputs_.changed();}};
  services_.skill.parseUserSpecifiedModel=[](const std::string& name){return name;};
  services_.skill.parseHooks=[](const Json& hooks)->std::optional<Json>{
    if(!hooks.is_object())return {};
    return hooks;};
  services_.skill.tryParseShellCommand=[](const std::string& raw)
      ->std::optional<std::vector<std::string>>{
    try{
      const auto tokens=shell_quote::parse(raw,
        [](const std::string& name)->std::optional<Json>{return Json("$"+name);});
      std::vector<std::string> words;
      for(const auto& token:tokens)if(token.is_string())
        words.push_back(token.get<std::string>());
      return words;
    }catch(...){return std::nullopt;}
  };
  services_.skill.executeShellCommandsInPoetry=[](const std::string& text,
      const std::string& command,const std::optional<std::string>&,
      const std::vector<std::string>&){
    if(containsPoetryShell(text))
      throw std::runtime_error("Prompt shell execution for "+command+
        " requires a native permission-bound Bash context");
    return text;};
  services_.skill.getFlightId=[flight=inputs_.flight,
      dispatcher=inputs_.owner.dispatcher()]{
    return application::onOwnerAndWait<std::string>(dispatcher,
      [flight]{return flight->id();});};
  services_.skill.logDebug=inputs_.diagnostic;
  services_.skill.logError=inputs_.diagnostic;
}
void Provider::bindRuntime(
    std::shared_ptr<application_media::AggregateProvider> attachments,
    std::shared_ptr<application_hooks::Provider> hooks){
  owner(inputs_.owner);
  if(!attachments||!hooks)throw std::invalid_argument(
    "Skill runtime needs attachment and hook providers");
  attachments_=std::move(attachments);hooks_=std::move(hooks);
}
void Provider::bindBatRunner(std::function<void(const bats::RunBatParams&,
    const query::Yield&)> run){
  owner(inputs_.owner);
  if(!run)throw std::invalid_argument("Skill Agent runner is required");
  runBat_=std::move(run);
}
void Provider::bindMcpPoetryRunner(std::function<Json(const Json&,
    const std::string&,const bats::BatContext&)> run){
  owner(inputs_.owner);
  if(!run)throw std::invalid_argument("MCP Skill prompt runner is required");
  mcpPoetry_=std::move(run);
}
std::vector<skills::SkillCommand> Provider::loadedSkills(){
  owner(inputs_.owner);
  auto result=skills::getSkillDirCommands(runtime::pathToUtf8(inputs_.config.cwd),
    *roster_,services_);
  const auto dynamic=skills::getDynamicSkills(*roster_);
  for(const auto& item:dynamic){
    const auto found=std::find_if(result.begin(),result.end(),[&](const auto& old){
      return old.name==item.name;});
    if(found==result.end())result.push_back(item);
    else *found=item;
  }
  return result;
}
Json Provider::descriptor(const skills::SkillCommand& command)const{
  Json result={{"type","prompt"},{"name",command.name},
    {"description",command.description},{"source",command.source},
    {"loadedFrom",command.loadedFrom},
    {"progressMessage",command.progressMessage},
    {"contentLength",command.contentLength},
    {"userInvocable",command.userInvocable},
    {"disableModelInvocation",command.disableModelInvocation},
    {"isHidden",command.isHidden},
    {"hasUserSpecifiedDescription",command.hasUserSpecifiedDescription},
    {"allowedTools",strings(command.allowedGadgets)},
    {"argNames",strings(command.argumentNames)},
    {"paths",strings(command.paths)}};
  if(command.displayName)result["userFacingName"]=*command.displayName;
  if(command.argumentHint)result["argumentHint"]=*command.argumentHint;
  if(command.model)result["model"]=*command.model;
  if(!command.effort.is_null())result["effort"]=command.effort;
  if(!command.whenToUse.is_null())result["whenToUse"]=command.whenToUse;
  if(!command.hooks.is_null())result["hooks"]=command.hooks;
  if(command.baseDir)result["skillRoot"]=*command.baseDir;
  if(command.executionContext)result["context"]=*command.executionContext;
  if(!command.bat.is_null())result["agent"]=command.bat;
  return result;
}
Json Provider::modelCommands(){
  owner(inputs_.owner);
  Json result=Json::array();
  for(const auto& command:loadedSkills()){
    commandIndex_[command.name]=command;
    if(!command.disableModelInvocation)result.push_back(descriptor(command));
  }
  if(inputs_.mcpCommands){
    const auto rows=inputs_.mcpCommands();
    if(rows.is_array())for(const auto& command:rows){
      if(value::property(command,"type")!="prompt"||
          value::truthy(value::property(command,"disableModelInvocation"))||
          value::property(command,"source")=="builtin")continue;
      const auto& loaded=value::property(command,"loadedFrom");
      const bool autoListed=loaded=="bundled"||loaded=="skills"||
        loaded=="commands_DEPRECATED";
      if(autoListed||value::truthy(value::property(command,"hasUserSpecifiedDescription"))||
          value::truthy(value::property(command,"whenToUse")))
        result.push_back(command);
    }
  }
  return result;
}
std::vector<Json> Provider::allCommands(){
  owner(inputs_.owner);
  std::vector<Json> result;
  for(const auto& command:loadedSkills()){
    commandIndex_[command.name]=command;
    result.push_back(descriptor(command));
  }
  return result;
}
Json Provider::findCommand(const std::string& name,
    const std::vector<Json>& all)const{
  for(const auto& command:all)
    if(value::property(command,"name")==name)return command;
  return nullptr;
}
void Provider::clearStashes(){
  owner(inputs_.owner);
  skills::clearSkillStashes(*roster_,services_);
  skills::clearDynamicSkills(*roster_);
  commandIndex_.clear();
  if(inputs_.changed)inputs_.changed();
}
}
