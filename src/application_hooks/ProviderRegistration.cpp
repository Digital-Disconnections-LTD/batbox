#include "application_hooks/Provider.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::application_hooks {
bool Provider::hasHooksFor(const std::string& event)const{
  if(!owner_.isOwnerThread())throw std::logic_error(
    "Hook configuration lookup requires owner loop");
  if(!settingsHooks(event).empty())return true;
  if(inputs_.getAppState&&!flightHooks(inputs_.getAppState(),event).empty())
    return true;
  const auto flight=inputs_.flightIdProvider?
    inputs_.flightIdProvider():inputs_.flightId;
  if(indigenousFlightStore_&&!indigenousFlightStore_->getFlightFunctionHooks(
      indigenousFlightHooks_,flight,event).empty())return true;
  const auto found=registered_.find(flight);
  if(found==registered_.end())return false;
  const auto current=found->second.find(event);
  return current!=found->second.end()&&!current->second.empty();
}
void Provider::registerFrontmatterHooks(const std::string& flightId,
    const Json& configured,const std::string& source,bool isBat,
    std::optional<std::string> skillRoot){
  if(!owner_.isOwnerThread())throw std::logic_error(
    "Frontmatter hook registration requires owner loop");
  if(!configured.is_object()||configured.empty())return;
  static const std::vector<std::string> events={
    "PreToolUse","PostToolUse","PostToolUseFailure","Notification",
    "UserPromptSubmit","SessionStart","SessionEnd","Stop","SubagentStart",
    "SubagentStop","PreCompact","PostCompact","PermissionRequest",
    "Setup","TeammateIdle","TaskCreated","TaskCompleted","ConfigChange","WorktreeCreate",
    "WorktreeRemove","PreModelCall","PostModelCall"};
  for(const auto& event:events){
    const auto& raw=value::property(configured,event);
    if(raw.is_null())continue;
    if(!raw.is_array())throw std::invalid_argument(
      "Frontmatter hooks for "+event+" must be an array");
    const auto target=isBat&&event=="Stop"?"SubagentStop":event;
    for(const auto& item:raw){
      if(!item.is_object()||!value::property(item,"hooks").is_array())
        throw std::invalid_argument("Frontmatter hook matcher requires hooks array");
      hooks::HookMatcher matcher;
      const auto& pattern=value::property(item,"matcher");
      if(pattern.is_string())matcher.matcher=pattern.get<std::string>();
      else if(!pattern.is_null())throw std::invalid_argument(
        "Frontmatter hook matcher must be a string");
      matcher.skillRoot=skillRoot;
      if(skillRoot)matcher.skillName=source;
      for(const auto& rawHook:item.at("hooks")){
        if(!rawHook.is_object())throw std::invalid_argument(
          "Frontmatter hook must be an object");
        const auto& kind=value::property(rawHook,"type");
        if(kind!="command"&&kind!="http"&&kind!="prompt"&&kind!="agent")throw std::invalid_argument(
          "Frontmatter hook kind requires a native provider");
        if(kind=="command"&&!value::property(rawHook,"command").is_string())
          throw std::invalid_argument("Frontmatter command hook requires command");
        if(kind=="http"&&!value::property(rawHook,"url").is_string())
          throw std::invalid_argument("Frontmatter HTTP hook requires URL");
        if(kind=="prompt"&&!value::property(rawHook,"prompt").is_string())
          throw std::invalid_argument("Frontmatter prompt hook requires prompt");
        if(kind=="agent"&&!value::property(rawHook,"prompt").is_string())
          throw std::invalid_argument("Frontmatter agent hook requires prompt");
        hooks::MatchedHook hook;
        hook.kind=kind=="http"?hooks::HookKind::Http:
          kind=="prompt"?hooks::HookKind::Poetry:
          kind=="agent"?hooks::HookKind::Bat:hooks::HookKind::Command;
        hook.hook=rawHook;
        if(value::truthy(value::property(rawHook,"once"))){
          hook.hook["_native_once"]=true;
          hook.hook["_native_session_key"]=inputs_.randomUUID();
        }
        matcher.hooks.push_back(std::move(hook));
      }
      if(!matcher.hooks.empty())
        registered_[flightId][target].push_back(std::move(matcher));
    }
  }
}
void Provider::clearFlightHooks(const std::string& flightId){
  if(!owner_.isOwnerThread())throw std::logic_error(
    "Session hook cleanup requires owner loop");
  registered_.erase(flightId);
  if(indigenousFlightStore_)indigenousFlightStore_->clearFlightHooks(flightId);
}
void Provider::removeOneShot(const std::string& flightId,
    const std::string& event,const std::string& id){
  const auto flight=registered_.find(flightId);
  if(flight==registered_.end())return;
  const auto found=flight->second.find(event);
  if(found==flight->second.end())return;
  auto& matchers=found->second;
  for(auto& matcher:matchers){
    auto& hooks=matcher.hooks;
    hooks.erase(std::remove_if(hooks.begin(),hooks.end(),[&](const auto& hook){
      return value::property(hook.hook,"_native_session_key")==id;
    }),hooks.end());
  }
  matchers.erase(std::remove_if(matchers.begin(),matchers.end(),
    [](const auto& matcher){return matcher.hooks.empty();}),matchers.end());
  if(matchers.empty())flight->second.erase(found);
  if(flight->second.empty())registered_.erase(flight);
}
}
