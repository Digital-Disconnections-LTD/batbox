#include "instructions/hooks/runtime/HookMatching.hpp"
#include "glob/Regex16.hpp"
#include "runtime/JsonValue.hpp"
#include <array>
#include <map>
#include <memory>
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
std::string field(const Json& value,const char* name){
  return value.is_object()&&value.contains(name)&&value.at(name).is_string()
    ?value.at(name).get<std::string>():"";
}
std::string baseName(const std::string& path){
  if(path.empty())return "";
  const auto separator=[](char value){
#ifdef _WIN32
    return value=='/'||value=='\\';
#else
    return value=='/';
#endif
  };
  std::size_t end=path.size();while(end&&separator(path[end-1]))--end;
  if(end==0)return "";
  std::size_t start=end;
  while(start&&!separator(path[start-1]))--start;
  return path.substr(start,end-start);
}
std::string queryFor(const Json& input){
  const auto event=field(input,"hook_event_name");
  if(event=="PreToolUse"||event=="PostToolUse"||
    event=="PostToolUseFailure"||event=="PermissionRequest"||
    event=="PermissionDenied")return field(input,"tool_name");
  if(event=="SessionStart"||event=="ConfigChange")return field(input,"source");
  if(event=="Setup"||event=="PreCompact"||event=="PostCompact")
    return field(input,"trigger");
  if(event=="Notification")return field(input,"notification_type");
  if(event=="SessionEnd")return field(input,"reason");
  if(event=="StopFailure")return field(input,"error");
  if(event=="SubagentStart"||event=="SubagentStop")
    return field(input,"agent_type");
  if(event=="Elicitation"||event=="ElicitationResult")
    return field(input,"mcp_server_name");
  if(event=="InstructionsLoaded")return field(input,"load_reason");
  if(event=="FileChanged")return baseName(field(input,"file_path"));
  return "";
}
bool simplePattern(const std::string& value){
  if(value.empty())return false;
  for(unsigned char c:value)
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
      (c>='0'&&c<='9')||c=='_'||c=='|'))return false;
  return true;
}
std::string dedupKey(const MatchedHook& hook,const std::string& payload){
  return (hook.pluginRoot?*hook.pluginRoot:
    hook.skillRoot?*hook.skillRoot:std::string{})+'\0'+payload;
}
std::string ifCondition(const MatchedHook& hook){return field(hook.hook,"if");}
bool gadgetEvent(const Json& input){
  const auto event=field(input,"hook_event_name");
  return event=="PreToolUse"||event=="PostToolUse"||
    event=="PostToolUseFailure"||event=="PermissionRequest";
}
std::vector<MatchedHook> deduplicate(const std::vector<MatchedHook>& rows,
  const std::string& defaultShell){
  constexpr std::array<HookKind,4> order={HookKind::Command,HookKind::Poetry,
    HookKind::Bat,HookKind::Http};
  std::vector<MatchedHook> result;
  for(const auto category:order){
    std::vector<MatchedHook> unique;
    std::map<std::string,std::size_t> indexes;
    for(const auto& row:rows){
      if(row.kind!=category)continue;
      std::string payload;
      if(category==HookKind::Command){
        const auto shell=field(row.hook,"shell");
        payload=(shell.empty()?defaultShell:shell)+'\0'+
          field(row.hook,"command")+'\0'+ifCondition(row);
      }else if(category==HookKind::Http)
        payload=field(row.hook,"url")+'\0'+ifCondition(row);
      else payload=field(row.hook,"prompt")+'\0'+ifCondition(row);
      const auto key=dedupKey(row,payload);
      const auto position=indexes.find(key);
      if(position==indexes.end()){
        indexes.emplace(key,unique.size());unique.push_back(row);
      }else unique[position->second]=row; // JS Map keeps first key position, last value.
    }
    result.insert(result.end(),unique.begin(),unique.end());
  }
  for(const auto& row:rows)if(row.kind==HookKind::Callback)result.push_back(row);
  for(const auto& row:rows)if(row.kind==HookKind::Function)result.push_back(row);
  return result;
}
struct MatchRun:std::enable_shared_from_this<MatchRun>{
  event_loop::Loop& loop;
  std::optional<Json> appState;
  std::string flightId,event;
  Json input;
  HookMatchServices services;
  event_loop::Deferred<std::vector<MatchedHook>> result;
  std::vector<MatchedHook> unique;
  std::size_t beforeDedup=0;
  std::string query;
  MatchRun(event_loop::Loop& owner,std::optional<Json> state,std::string flight,
    std::string hookEvent,Json hookInput,HookMatchServices imports):loop(owner),
    appState(std::move(state)),flightId(std::move(flight)),
    event(std::move(hookEvent)),input(std::move(hookInput)),
    services(std::move(imports)),result(owner){}
  void debug(const std::string& message,const std::string& level="info"){
    if(!services.logDebug)throw std::invalid_argument("Hook matcher debug import required");
    services.logDebug(message,level);
  }
  void doneEmpty(){result.resolve({});}
  void start(){
    try{
      if(!services.getHooksConfig)
        throw std::invalid_argument("Hook settings config import required");
      const auto matchers=services.getHooksConfig(appState,flightId,event);
      query=queryFor(input);
      debug("Getting matching hook commands for "+event+
        " with query: "+(query.empty()?"undefined":query),"verbose");
      debug("Found "+std::to_string(matchers.size())+" hook matchers in settings",
        "verbose");
      std::vector<MatchedHook> matched;
      for(const auto& matcher:matchers){
        if(!query.empty()&&matcher.matcher&&!matcher.matcher->empty()&&
          !matchesHookPattern(query,*matcher.matcher,services))continue;
        for(auto row:matcher.hooks){
          row.pluginRoot=matcher.pluginRoot;row.pluginId=matcher.pluginId;
          row.skillRoot=matcher.skillRoot;
          row.hookSource=matcher.pluginRoot&&!matcher.pluginRoot->empty()
            ?(matcher.pluginName?"plugin:"+*matcher.pluginName:"plugin")
            :matcher.skillRoot&&!matcher.skillRoot->empty()
              ?(matcher.skillName?"skill:"+*matcher.skillName:"skill")
              :"settings";
          matched.push_back(std::move(row));
        }
      }
      beforeDedup=matched.size();
      bool callbacksOnly=true;
      for(const auto& row:matched)
        if(row.kind!=HookKind::Callback&&row.kind!=HookKind::Function){
          callbacksOnly=false;break;}
      if(callbacksOnly){result.resolve(std::move(matched));return;}
      unique=deduplicate(matched,"bash");
      bool hasIf=false;
      for(const auto& row:unique)
        if(row.kind!=HookKind::Callback&&row.kind!=HookKind::Function&&
          !ifCondition(row).empty()){hasIf=true;break;}
      if(!hasIf){afterIf(std::nullopt);return;}
      if(!gadgetEvent(input)){
        afterIf(std::nullopt);return;
      }
      if(!services.prepareIfConditionMatcher)
        throw std::invalid_argument("Hook tool-condition matcher import required");
      services.prepareIfConditionMatcher(input).then(
        [self=shared_from_this()](const std::optional<IfConditionMatcher>* matcher,
          const std::exception_ptr& error){
          if(error){self->doneEmpty();return;}
          try{self->afterIf(*matcher);}catch(...){self->doneEmpty();}
        });
    }catch(...){doneEmpty();}
  }
  void afterIf(const std::optional<IfConditionMatcher>& matcher){
    std::vector<MatchedHook> filtered;
    for(const auto& row:unique){
      if(row.kind!=HookKind::Callback&&row.kind!=HookKind::Function){
        const auto condition=ifCondition(row);
        if(!condition.empty()){
          if(!matcher){
            debug("Hook if condition \""+condition+
              "\" cannot be evaluated for non-tool event "+
              field(input,"hook_event_name"));continue;
          }
          if(!(*matcher)(condition)){
            debug("Skipping hook due to if condition \""+condition+
              "\" not matching");continue;
          }
        }
      }
      if((event=="SessionStart"||event=="Setup")&&row.kind==HookKind::Http){
        debug("Skipping HTTP hook "+field(row.hook,"url")+
          " — HTTP hooks are not supported for "+event);continue;
      }
      filtered.push_back(row);
    }
    debug("Matched "+std::to_string(filtered.size())+
      " unique hooks for query \""+(query.empty()?"no match query":query)+
      "\" ("+std::to_string(beforeDedup)+" before deduplication)","verbose");
    result.resolve(std::move(filtered));
  }
};
}
bool matchesHookPattern(const std::string& query,const std::string& matcher,
  const HookMatchServices& services){
  if(matcher.empty()||matcher=="*")return true;
  if(simplePattern(matcher)){
    if(!services.normalizeLegacyGadgetName)
      throw std::invalid_argument("Hook legacy-name normalization import required");
    std::size_t start=0;
    do{
      const auto end=matcher.find('|',start);
      const auto part=value::trim(matcher.substr(start,end==std::string::npos
        ?std::string::npos:end-start));
      if(query==services.normalizeLegacyGadgetName(part))return true;
      if(end==std::string::npos)break;
      start=end+1;
    }while(true);
    return false;
  }
  try{
    glob::Regex16 regex(glob::units(matcher));
    if(regex.search(glob::units(query)))return true;
    if(!services.getLegacyGadgetNames)
      throw std::invalid_argument("Hook legacy-name lookup import required");
    for(const auto& legacy:services.getLegacyGadgetNames(query))
      if(regex.search(glob::units(legacy)))return true;
    return false;
  }catch(const std::invalid_argument&){
    if(!services.logDebug)throw;
    services.logDebug("Invalid regex pattern in hook matcher: "+matcher,"info");
    return false;
  }
}
event_loop::Deferred<std::vector<MatchedHook>> getMatchingHooks(
  event_loop::Loop& loop,const std::optional<Json>& appState,
  const std::string& flightId,const std::string& hookEvent,
  const Json& hookInput,const HookMatchServices& services){
  if(!loop.isOwnerThread())throw std::logic_error("Hook matcher owner required");
  auto run=std::make_shared<MatchRun>(loop,appState,flightId,hookEvent,
    hookInput,services);
  run->start();return run->result;
}
void bindHookMatching(event_loop::Loop& loop,ExecutorServices& executor,
  HookMatchServices services){
  executor.getMatchingHooks=[&loop,services](const std::string& flight,
    const std::string& event,const Json& input){
    return getMatchingHooks(loop,std::nullopt,flight,event,input,services);
  };
  executor.getMatchingHooksWithAppState=[&loop,services](const Json& state,
    const std::string& flight,const std::string& event,const Json& input){
    return getMatchingHooks(loop,state,flight,event,input,services);
  };
}
} // namespace batbox::instructions::hooks::runtime
