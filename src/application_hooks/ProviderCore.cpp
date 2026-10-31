#include "application_hooks/Provider.hpp"
#include "instructions/hooks/runtime/HookSchema.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/Platform.hpp"
#ifdef _WIN32
#include "shell/WindowsPaths.hpp"
#endif
#ifdef _WIN32
#include "runtime/IndigenousUnicode.hpp"
#endif
#include "flights/ProjectDirectories.hpp"
#include <atomic>
#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <map>
#include <random>
#include <stdexcept>

#ifndef _WIN32
extern char** environ;
#endif
namespace batbox::application_hooks {
namespace {
template<class T>event_loop::Deferred<T> ready(event_loop::Loop& owner,T value){
  event_loop::Deferred<T> output(owner);output.resolve(std::move(value));return output;
}
std::string stringField(const Json& object,const char* key){
  const auto& found=value::property(object,key);
  return found.is_string()?found.get<std::string>():std::string{};
}
std::vector<hooks::HookMatcher> parseMatchers(const Json& settings,
    const std::string& event){
  if(!settings.is_object()||!settings.contains(event))return {};
  const auto& entries=settings.at(event);
  if(!entries.is_array())throw std::invalid_argument(
    "Configured hooks for "+event+" must be an array");
  std::vector<hooks::HookMatcher> output;
  for(const auto& entry:entries){
    if(!entry.is_object())throw std::invalid_argument("Hook matcher must be an object");
    hooks::HookMatcher matcher;
    if(entry.contains("matcher")){
      if(!entry.at("matcher").is_string())
        throw std::invalid_argument("Hook matcher pattern must be a string");
      matcher.matcher=entry.at("matcher").get<std::string>();
    }
    if(!entry.contains("hooks")||!entry.at("hooks").is_array())
      throw std::invalid_argument("Hook matcher requires hooks array");
    for(const auto& hook:entry.at("hooks")){
      if(!hook.is_object())throw std::invalid_argument("Hook entry must be an object");
      const auto type=stringField(hook,"type");
      if(type!="command"&&type!="http"&&type!="prompt"&&type!="agent")throw std::invalid_argument(
        "Configured "+type+" hooks need a native provider");
      if(type=="command"&&(!hook.contains("command")||
          !hook.at("command").is_string()))
        throw std::invalid_argument("Command hook requires command string");
      if(type=="http"&&(!hook.contains("url")||
          !hook.at("url").is_string()))
        throw std::invalid_argument("HTTP hook requires URL string");
      if(type=="prompt"&&(!hook.contains("prompt")||
          !hook.at("prompt").is_string()))
        throw std::invalid_argument("Prompt hook requires prompt string");
      if(type=="agent"&&(!hook.contains("prompt")||
          !hook.at("prompt").is_string()))
        throw std::invalid_argument("Agent hook requires prompt string");
      if(type=="http"&&hook.contains("headers")){
        if(!hook.at("headers").is_object())throw std::invalid_argument(
          "HTTP hook headers must be an object");
        for(auto it=hook.at("headers").begin();it!=hook.at("headers").end();++it)
          if(!it.value().is_string())throw std::invalid_argument(
            "HTTP hook header values must be strings");
      }
      if(type=="http"&&hook.contains("allowedEnvVars")){
        if(!hook.at("allowedEnvVars").is_array())throw std::invalid_argument(
          "HTTP hook allowedEnvVars must be an array");
        for(const auto& name:hook.at("allowedEnvVars"))
          if(!name.is_string())throw std::invalid_argument(
            "HTTP hook allowedEnvVars entries must be strings");
      }
      if(hook.contains("if"))throw std::invalid_argument(
        "Configured hook if conditions need a native permission matcher");
      hooks::MatchedHook row;row.kind=type=="http"?
        hooks::HookKind::Http:type=="prompt"?
        hooks::HookKind::Poetry:type=="agent"?
        hooks::HookKind::Bat:hooks::HookKind::Command;row.hook=hook;
      matcher.hooks.push_back(std::move(row));
    }
    output.push_back(std::move(matcher));
  }
  return output;
}
std::map<std::string,std::string> environment(){
  std::map<std::string,std::string> values;
#ifdef _WIN32
  auto* block=GetEnvironmentStringsW();
  if(!block)throw std::runtime_error("Cannot read Windows process environment");
  struct Release {
    wchar_t* value;
    ~Release(){FreeEnvironmentStringsW(value);}
  } release{block};
  for(const wchar_t* item=block;*item;item+=std::wcslen(item)+1){
    const std::wstring text(item);
    if(text.empty()||text.front()==L'=')continue;
    const auto equal=text.find(L'=');
    if(equal!=std::wstring::npos)
      values[runtime::wideToUtf8(text.substr(0,equal))]=
        runtime::wideToUtf8(text.substr(equal+1));
  }
#else
  for(char** item=::environ;item&&*item;++item){
    const std::string text=*item;const auto equal=text.find('=');
    if(equal!=std::string::npos)values[text.substr(0,equal)]=text.substr(equal+1);
  }
#endif
  return values;
}
std::string errorMessage(std::exception_ptr error){
  try{if(error)std::rethrow_exception(error);}catch(const std::exception& ex){
    return ex.what();}catch(...){return "unknown error";}return {};
}
}
Provider::Provider(event_loop::Loop& owner,Inputs inputs):owner_(owner),
    inputs_(std::move(inputs)){
  if(inputs_.flightId.empty()&&!inputs_.flightIdProvider)
    throw std::invalid_argument("Hook session ID required");
  if(!inputs_.log)inputs_.log=[](const std::string&){};
  if(!inputs_.randomUUID){
    auto counter=std::make_shared<std::atomic<std::uint64_t>>(0);
    inputs_.randomUUID=[counter]{return "hook-"+
      std::to_string(++*counter);};
  }
  if(!inputs_.createAttachmentMessage)
    inputs_.createAttachmentMessage=[](const Json& attachment){
      return Json{{"type","attachment"},{"attachment",attachment}};};
  configure();
}
std::vector<hooks::HookMatcher> Provider::settingsHooks(
    const std::string& event)const{
  const auto state=inputs_.getAppState?inputs_.getAppState():Json();
  const auto& live=value::property(state,"settings");
  const auto& settings=live.is_object()?live:inputs_.settings;
  const auto& root=value::property(settings,"hooks");
  return parseMatchers(root.is_object()?root:settings,event);
}
std::vector<hooks::HookMatcher> Provider::flightHooks(
    const Json& state,const std::string& event)const{
  return parseMatchers(value::property(state,"sessionHooks"),event);
}
Json Provider::baseInput(const std::optional<std::string>& mode)const{
  return hooks::createBaseHookInput(baseServices_,mode).toJson();
}
void Provider::configure(){
  hooks::FlightHookStoreImports flightImports;
  flightImports.setAppState=[this](const auto& update){
    update(indigenousFlightHooks_);};
  flightImports.logForDebugging=inputs_.log;
  flightImports.dateNow=[]{return static_cast<double>(platform::nowMillis());};
  flightImports.random=[]{
    static thread_local std::mt19937_64 generator(std::random_device{}());
    return std::generate_canonical<double,53>(generator);};
  flightImports.defaultHookShell="sh";
  flightImports.hookEvents={"PreToolUse","PostToolUse","PostToolUseFailure",
    "Notification","UserPromptSubmit","SessionStart","SessionEnd","Stop",
    "SubagentStart","SubagentStop","PreCompact","PostCompact",
    "PermissionRequest","Setup","TeammateIdle","TaskCreated","TaskCompleted",
    "ConfigChange","WorktreeCreate","WorktreeRemove","FileChanged",
    "CwdChanged"};
  indigenousFlightStore_=std::make_shared<hooks::FlightHookStore>(
    std::move(flightImports));
  baseServices_.getFlightId=[this]{return inputs_.flightIdProvider?
    inputs_.flightIdProvider():inputs_.flightId;};
  baseServices_.getCwd=[this]{return runtime::pathToUtf8(inputs_.config.cwd);};
  baseServices_.getMainThreadBatType=[]()->std::optional<std::string>{return {};};
  if(inputs_.transcriptPathForFlight)
    baseServices_.getTranscriptPathForFlight=
      inputs_.transcriptPathForFlight;
  else{
    auto projects=std::make_shared<flights::ProjectDirectories>(
      [home=inputs_.config.stateDirectory]{return runtime::pathToUtf8(home);});
    baseServices_.getTranscriptPathForFlight=
      [projects,cwd=runtime::pathToUtf8(inputs_.config.cwd)](
          const std::string& id){
        return runtime::pathToUtf8(
          runtime::pathFromUtf8(projects->getProjectDirPortable(cwd))/
          (id+".jsonl"));};
  }
  configServices_.getSnapshotHooks=[this](const std::string& event)
      ->std::optional<std::vector<hooks::HookMatcher>>{
    return settingsHooks(event);};
  configServices_.getRegisteredHooks=[](const std::string&)
      ->std::optional<std::vector<hooks::HookMatcher>>{return {};};
  configServices_.managedOnly=[](){return false;};
  configServices_.getFlightHooks=[this](const Json& state,
      const std::string& flight,const std::string& event)
      ->std::optional<std::vector<hooks::HookMatcher>>{
    auto matches=flightHooks(state,event);
    if(const auto found=registered_.find(flight);found!=registered_.end())
      if(const auto rows=found->second.find(event);rows!=found->second.end())
        matches.insert(matches.end(),rows->second.begin(),rows->second.end());
    return matches;};
  configServices_.getFlightFunctionHooks=[this](const Json&,
      const std::string& flight,const std::string& event)
      ->std::optional<std::vector<hooks::HookMatcher>>{
    std::vector<hooks::HookMatcher> matches;
    for(const auto& [_,group]:indigenousFlightStore_->getFlightFunctionHooks(
        indigenousFlightHooks_,flight,event))for(const auto& item:group){
      hooks::HookMatcher matcher;
      matcher.matcher=item.matcher;
      for(const auto& value:item.hooks){
        hooks::MatchedHook row;
        row.kind=hooks::HookKind::Function;
        row.hook=value->fields;
        row.function=value->function;
        matcher.hooks.push_back(std::move(row));
      }
      matches.push_back(std::move(matcher));
    }
    return matches;};
  hooks::bindHookConfig(matcher_,configServices_);
  matcher_.normalizeLegacyGadgetName=[](const std::string& value){return value;};
  matcher_.getLegacyGadgetNames=[](const std::string&){return std::vector<std::string>{};};
  matcher_.prepareIfConditionMatcher=[this](const Json&){
    return ready(owner_,std::optional<hooks::IfConditionMatcher>{});};
  matcher_.logDebug=[this](const std::string& message,const std::string&){
    inputs_.log(message);};
  command_.getProjectRoot=[this]{return runtime::pathToUtf8(inputs_.config.cwd);};
  command_.getCwd=command_.getProjectRoot;
  command_.getOriginalCwd=command_.getProjectRoot;
#ifdef _WIN32
  command_.shellPath=shell::findGitBashPath();
#endif
  command_.subprocessEnv=environment;
  command_.pathExists=[this](const std::string& path){
    std::error_code error;return ready(owner_,
      std::filesystem::exists(runtime::pathFromUtf8(path),error));};
  command_.shellPrefix=[]()->std::optional<std::string>{return {};};
  command_.defaultTimeoutMs=inputs_.defaultTimeoutMs;
  command_.nowMillis=[] {return static_cast<double>(platform::nowMillis());};
  command_.logDebug=[this](const std::string& message,const std::string&){
    inputs_.log(message);};
  command_.logDiagnostics=[this](const std::string& name,const Json& detail){
    inputs_.log("Hook "+name+": "+detail.dump());};
  auto flightEnvironment=hooks::indigenousFlightEnvironmentImports(owner_,
    [this]{return runtime::pathToUtf8(inputs_.config.stateDirectory);},
    baseServices_.getFlightId,
#ifdef _WIN32
    []{return std::string("windows");},
#else
    []{return std::string("posix");},
#endif
    []()->std::optional<std::string>{
      const auto values=environment();
      const auto found=values.find("BATBOX_ENV_FILE");
      if(found==values.end()||found->second.empty())return std::nullopt;
      return found->second;},
    [this](const std::string& message){inputs_.log(message);});
  flightEnvironment_=std::make_shared<hooks::FlightEnvironment>(owner_,
    std::move(flightEnvironment));
  command_.getHookEnvFilePath=[environment=flightEnvironment_](
      const std::string& event,std::size_t index){
    return environment->getHookEnvFilePath(event,index);};
  parser_.logDebug=inputs_.log;
  parser_.stringifyThrown=errorMessage;
  hooks::bindHookOutputSchema(parser_,{});
  executor_.simpleMode=[](){return false;};
  executor_.allHooksDisabled=[](){return false;};
  executor_.skipDueToTrust=[](){return false;};
  executor_.getFlightId=baseServices_.getFlightId;
  executor_.randomUUID=inputs_.randomUUID;
  bindCommandHooks();
  bindHttpHooks();
  bindPoetryHooks();
  bindFunctionHooks();
  bindBatHooks();
  executor_.logDebug=matcher_.logDebug;
  executor_.logError=inputs_.log;
  hooks::bindHookMatching(owner_,executor_,matcher_);
  hooks::bindHookOutputParsers(executor_,parser_);
  compact_=hooks::bindCompactHookServices(owner_,baseServices_,executor_,
    inputs_.defaultTimeoutMs);
}
} // namespace batbox::application_hooks
