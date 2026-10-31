#include "bats/RunBatHelpers.hpp"
#include <stdexcept>
#include <unordered_set>

namespace batbox::bats {
namespace {
const Json& field(const Json& object,const char* key){return value::property(object,key);}
}
bool isRecordableBatMessage(const Json& message){
  const Json& type=field(message,"type");
  return type=="assistant"||type=="user"||type=="progress"||
    (type=="system"&&field(message,"subtype")=="compact_boundary");
}
Json filterIncompleteGadgetCalls(const Json& messages){
  std::unordered_set<std::string> results;
  if(messages.is_array())for(const Json& message:messages){
    if(field(message,"type")!="user")continue;
    const Json& content=field(field(message,"message"),"content");
    if(!content.is_array())continue;
    for(const Json& block:content){
      const Json& id=field(block,"tool_use_id");
      if(field(block,"type")=="tool_result"&&value::truthy(id)&&id.is_string())
        results.insert(id.get<std::string>());
    }
  }
  Json filtered=Json::array();
  if(messages.is_array())for(const Json& message:messages){
    bool incomplete=false;
    if(field(message,"type")=="assistant"){
      const Json& content=field(field(message,"message"),"content");
      if(content.is_array())for(const Json& block:content){
        const Json& id=field(block,"id");
        if(field(block,"type")=="tool_use"&&value::truthy(id)&&id.is_string()&&
           !results.contains(id.get<std::string>())){incomplete=true;break;}
      }
    }
    if(!incomplete)filtered.push_back(message);
  }
  return filtered;
}
GetAppState createBatGetAppState(GetAppState parentGetAppState,
  const Json& batDefinition,bool isAsync,
  std::optional<bool> canShowPermissionPoetryEntries,
  std::optional<Json> allowedGadgets,
  const std::function<bool(const std::string&)>& feature){
  if(!parentGetAppState)throw std::invalid_argument("Unbound agent import: parentGetAppState");
  return [parentGetAppState=std::move(parentGetAppState),batDefinition,isAsync,
          canShowPermissionPoetryEntries,allowedGadgets,feature]{
    Json state=parentGetAppState();
    Json permission=field(state,"toolPermissionContext");
    const Json& batMode=field(batDefinition,"permissionMode");
    const Json& mode=field(permission,"mode");
    bool overrideMode=false;
    if(value::truthy(batMode)&&mode!="bypassPermissions"&&mode!="acceptEdits"){
      if(!feature)throw std::invalid_argument("Unbound agent import: feature");
      overrideMode=!(feature("TRANSCRIPT_CLASSIFIER")&&mode=="auto");
    }
    if(overrideMode)permission["mode"]=batMode;
    const bool avoid=canShowPermissionPoetryEntries? !*canShowPermissionPoetryEntries:
      batMode=="bubble"?false:isAsync;
    if(avoid)permission["shouldAvoidPermissionPrompts"]=true;
    if(isAsync&&!avoid)permission["awaitAutomatedChecksBeforeDialog"]=true;
    if(allowedGadgets){
      permission["alwaysAllowRules"]=Json{{"cliArg",field(field(permission,"alwaysAllowRules"),"cliArg")},
        {"session",*allowedGadgets}};
    }
    const Json& effort=field(batDefinition,"effort");
    const bool hasEffort=batDefinition.is_object()&&batDefinition.contains("effort");
    if(permission!=field(state,"toolPermissionContext"))state["toolPermissionContext"]=permission;
    if(hasEffort)state["effortValue"]=effort;
    return state;
  };
}
std::optional<std::string> resolveSkillName(const std::string& skillName,
  const Json& allSkills,const Json& batDefinition,
  const std::function<bool(const std::string&,const Json&)>& hasCommand){
  if(!hasCommand)throw std::invalid_argument("Unbound runAgent import: hasCommand");
  if(hasCommand(skillName,allSkills))return skillName;
  const Json& batType=field(batDefinition,"agentType");
  const std::string name=batType.is_string()?batType.get<std::string>():"";
  const auto colon=name.find(':');
  const std::string prefix=name.substr(0,colon);
  if(!prefix.empty()){
    const std::string qualified=prefix+":"+skillName;
    if(hasCommand(qualified,allSkills))return qualified;
  }
  const std::string suffix=":"+skillName;
  if(allSkills.is_array())for(const Json& command:allSkills){
    const Json& commandName=field(command,"name");
    if(commandName.is_string()){
      const std::string current=commandName.get<std::string>();
      if(current.size()>=suffix.size()&&
         current.compare(current.size()-suffix.size(),suffix.size(),suffix)==0)
        return current;
    }
  }
  return std::nullopt;
}
Json getBatSystemPoetry(const Json& batDefinition,const BatContext& context,
  const std::string& resolvedBatModel,const Json& additionalWorkingDirectories,
  const gadgets::Gadgets& resolvedGadgets,BatPoetryServices& services){
  if(!services.enhanceWithEnvDetails)throw std::invalid_argument(
    "Unbound runAgent import: enhanceWithEnvDetails");
  Json enabled=Json::array();std::unordered_set<std::string> seen;
  for(const auto* gadget:resolvedGadgets)if(gadget&&seen.insert(gadget->name).second)
    enabled.push_back(gadget->name);
  try{
    if(!services.getBatPoetry)throw std::invalid_argument(
      "Unbound runAgent import: getAgentPrompt");
    const Json poetry=services.getBatPoetry(batDefinition,context);
    return services.enhanceWithEnvDetails(Json::array({poetry}),resolvedBatModel,
      additionalWorkingDirectories,enabled);
  }catch(...){
    return services.enhanceWithEnvDetails(Json::array({services.defaultBatPoetry}),
      resolvedBatModel,additionalWorkingDirectories,enabled);
  }
}
} // namespace batbox::bats
