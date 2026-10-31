#include "instructions/hooks/runtime/ProcessHookJsonInternal.hpp"
#include "runtime/ArrayJoin.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime::detail {
std::string stringValue(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_null())return "null";
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_array())return value::arrayJoin(value,",");
  return "[object Object]";
}
std::string fieldString(const Json& object,const std::string& key){
  if(!object.is_object()||!object.contains(key))return "undefined";
  return stringValue(object.at(key));
}
void assignOptional(ProcessHookJsonResult& result,const std::string& name,
  const Json& object,const std::string& key){
  if(object.is_object()&&object.contains(key))result.fields[name]=object.at(key);
  else{
    result.fields.erase(name);
    result.ownUndefinedKeys.push_back(name);
  }
}
void block(ProcessHookJsonResult& result,const std::string& reason,
  const std::string& command){
  result.fields["blockingError"]={{"blockingError",reason},{"command",command}};
}
void processCommon(const ProcessHookJsonInput& input,
  ProcessHookJsonResult& result){
  const auto& json=input.json;
  if(json.contains("continue")&&json.at("continue")==false){
    result.fields["preventContinuation"]=true;
    if(json.contains("stopReason")&&value::truthy(json.at("stopReason")))
      result.fields["stopReason"]=json.at("stopReason");
  }
  if(json.contains("decision")&&value::truthy(json.at("decision"))){
    const auto decision=fieldString(json,"decision");
    if(decision=="approve")result.fields["permissionBehavior"]="allow";
    else if(decision=="block"){
      result.fields["permissionBehavior"]="deny";
      block(result,json.contains("reason")&&value::truthy(json.at("reason"))
        ?fieldString(json,"reason"):"Blocked by hook",input.command);
    }else throw std::runtime_error("Unknown hook decision type: "+decision+
      ". Valid types are: approve, block");
  }
  if(json.contains("systemMessage")&&value::truthy(json.at("systemMessage")))
    result.fields["systemMessage"]=json.at("systemMessage");
  if(json.contains("hookSpecificOutput")&&
    json.at("hookSpecificOutput").is_object()){
    const auto& specific=json.at("hookSpecificOutput");
    if(fieldString(specific,"hookEventName")=="PreToolUse"&&
      specific.contains("permissionDecision")&&
      value::truthy(specific.at("permissionDecision"))){
      const auto permission=fieldString(specific,"permissionDecision");
      if(permission=="allow")result.fields["permissionBehavior"]="allow";
      else if(permission=="deny"){
        result.fields["permissionBehavior"]="deny";
        block(result,json.contains("reason")&&value::truthy(json.at("reason"))
          ?fieldString(json,"reason"):"Blocked by hook",input.command);
      }else if(permission=="ask")result.fields["permissionBehavior"]="ask";
      else throw std::runtime_error("Unknown hook permissionDecision type: "+
        permission+". Valid types are: allow, deny, ask");
    }
  }
  if(result.fields.contains("permissionBehavior")&&json.contains("reason"))
    result.fields["hookPermissionDecisionReason"]=json.at("reason");
}
} // namespace batbox::instructions::hooks::runtime::detail
