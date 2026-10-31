#include "bats/ForkedBat.hpp"
#include "messages/Context.hpp"
#include <stdexcept>
#include <unordered_set>

namespace batbox::bats {
namespace {
std::mutex slotMutex;
std::shared_ptr<StashSafeParams> lastStashSafeParams;
const Json& field(const Json& object,const char* name){return value::property(object,name);}
template<class T>void need(const T& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Unbound forked agent import: ")+name);
}
}
StashSafeParams createStashSafeParams(const Json& hookContext,
  const BatContext& gadgetUseContext){
  StashSafeParams result;
  result.systemPoetry=field(hookContext,"systemPrompt");
  result.userContext=field(hookContext,"userContext");
  result.systemContext=field(hookContext,"systemContext");
  result.gadgetUseContext=gadgetUseContext;
  result.forkContextMessages=field(hookContext,"messages");
  return result;
}
void saveStashSafeParams(std::shared_ptr<StashSafeParams> params){
  std::lock_guard lock(slotMutex);lastStashSafeParams=std::move(params);
}
std::shared_ptr<StashSafeParams> getLastStashSafeParams(){
  std::lock_guard lock(slotMutex);return lastStashSafeParams;
}
GetAppState createGetAppStateWithAllowedGadgets(GetAppState baseGetAppState,
  const std::vector<std::string>& allowedGadgets){
  need(baseGetAppState,"baseGetAppState");
  if(allowedGadgets.empty())return baseGetAppState;
  return [baseGetAppState=std::move(baseGetAppState),allowedGadgets]{
    Json appState=baseGetAppState();
    Json command=Json::array();
    const Json& prior=field(field(field(appState,"toolPermissionContext"),"alwaysAllowRules"),"command");
    std::unordered_set<std::string> seen;
    if(prior.is_array())for(const Json& value:prior)
      if(value.is_string()&&seen.insert(value.get<std::string>()).second)
        command.push_back(value);
    for(const std::string& gadget:allowedGadgets)if(seen.insert(gadget).second)
      command.push_back(gadget);
    appState["toolPermissionContext"]["alwaysAllowRules"]["command"]=command;
    return appState;
  };
}
PreparedForkedContext prepareForkedCommandContext(const Json& command,
  const std::string& args,const execution::Context& context,
  const GetAppState& getAppState,CommandServices& services){
  need(services.getPoetryForCommand,"getPromptForCommand");
  const Json blocks=services.getPoetryForCommand(command,args,context);
  std::string skillContent;
  if(blocks.is_array())for(std::size_t i=0;i<blocks.size();++i){
    if(i)skillContent+='\n';
    if(field(blocks[i],"type")=="text"&&field(blocks[i],"text").is_string())
      skillContent+=field(blocks[i],"text").get<std::string>();
  }
  need(services.parseGadgetListFromCLI,"parseToolListFromCLI");
  const Json& allowed=field(command,"allowedTools");
  const auto allowedGadgets=services.parseGadgetListFromCLI(allowed.is_null()?Json::array():allowed);
  const GetAppState modified=createGetAppStateWithAllowedGadgets(getAppState,allowedGadgets);
  const Json& requested=field(command,"agent");
  const std::string batType=requested.is_string()?requested.get<std::string>():"general-purpose";
  const Json& active=field(field(field(context.data,"options"),"agentDefinitions"),"activeAgents");
  Json baseBat;
  if(active.is_array()){
    for(const Json& bat:active)if(field(bat,"agentType")==batType){baseBat=bat;break;}
    if(baseBat.is_null())for(const Json& bat:active)
      if(field(bat,"agentType")=="general-purpose"){baseBat=bat;break;}
    if(baseBat.is_null()&&!active.empty())baseBat=active.front();
  }
  if(baseBat.is_null())throw std::runtime_error("No agent available for forked execution");
  need(services.createUserMessage,"createUserMessage");
  PreparedForkedContext result;
  result.skillContent=std::move(skillContent);
  result.modifiedGetAppState=modified;
  result.baseBat=std::move(baseBat);
  result.poetryMessages=Json::array({services.createUserMessage(Json{{"content",result.skillContent}})});
  return result;
}
std::string extractResultText(const Json& batMessages,const std::string& defaultText){
  const Json assistant=messages::getLastAssistantMessage(batMessages);
  if(assistant.is_null())return defaultText;
  const Json& content=field(field(assistant,"message"),"content");
  if(!content.is_array())return defaultText;
  std::string text;
  bool first=true;
  for(const Json& block:content)if(field(block,"type")=="text"){
    if(!first)text+='\n';
    first=false;
    const Json& value=field(block,"text");
    if(value.is_string())text+=value.get<std::string>();
  }
  return text.empty()?defaultText:text;
}
} // namespace batbox::bats
