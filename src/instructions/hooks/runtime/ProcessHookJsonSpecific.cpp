#include "instructions/hooks/runtime/ProcessHookJsonInternal.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime::detail {
namespace {
std::string reasonOr(const Json& json,const std::string& fallback){
  return json.contains("reason")&&value::truthy(json.at("reason"))
    ?fieldString(json,"reason"):fallback;
}
void addElicitation(const ProcessHookJsonInput& input,
  ProcessHookJsonResult& result,const Json& specific,
  const std::string& field,const std::string& fallback){
  if(!specific.contains("action")||!value::truthy(specific.at("action")))return;
  Json response={{"action",specific.at("action")}};
  if(specific.contains("content"))response["content"]=specific.at("content");
  else result.ownUndefinedKeys.push_back(field+".content");
  result.fields[field]=std::move(response);
  if(specific.at("action")=="decline")
    block(result,reasonOr(input.json,fallback),input.command);
}
}
void processSpecific(const ProcessHookJsonInput& input,
  ProcessHookJsonResult& result){
  const auto& json=input.json;
  if(!json.contains("hookSpecificOutput")||
    !value::truthy(json.at("hookSpecificOutput")))return;
  const auto& specific=json.at("hookSpecificOutput");
  const auto event=fieldString(specific,"hookEventName");
  if(input.expectedHookEvent&&!input.expectedHookEvent->empty()&&
    event!=*input.expectedHookEvent)
    throw std::runtime_error("Hook returned incorrect event name: expected '"+
      *input.expectedHookEvent+"' but got '"+event+"'. Full stdout: "+
      value::stringify(json,2));
  if(event=="PreToolUse"){
    if(specific.contains("permissionDecision")&&
      value::truthy(specific.at("permissionDecision"))){
      const auto decision=fieldString(specific,"permissionDecision");
      if(decision=="allow")result.fields["permissionBehavior"]="allow";
      else if(decision=="deny"){
        result.fields["permissionBehavior"]="deny";
        const auto specificReason=specific.contains("permissionDecisionReason")&&
          value::truthy(specific.at("permissionDecisionReason"))
          ?fieldString(specific,"permissionDecisionReason"):
          reasonOr(json,"Blocked by hook");
        block(result,specificReason,input.command);
      }else if(decision=="ask")result.fields["permissionBehavior"]="ask";
    }
    assignOptional(result,"hookPermissionDecisionReason",specific,
      "permissionDecisionReason");
    if(specific.contains("updatedInput")&&value::truthy(specific.at("updatedInput")))
      result.fields["updatedInput"]=specific.at("updatedInput");
    assignOptional(result,"additionalContext",specific,"additionalContext");
    return;
  }
  if(event=="UserPromptSubmit"||event=="Setup"||
    event=="SubagentStart"||event=="PostToolUseFailure"){
    assignOptional(result,"additionalContext",specific,"additionalContext");
    return;
  }
  if(event=="SessionStart"){
    assignOptional(result,"additionalContext",specific,"additionalContext");
    assignOptional(result,"initialUserMessage",specific,"initialUserMessage");
    if(specific.contains("watchPaths")&&value::truthy(specific.at("watchPaths")))
      result.fields["watchPaths"]=specific.at("watchPaths");
    return;
  }
  if(event=="PostToolUse"){
    assignOptional(result,"additionalContext",specific,"additionalContext");
    if(specific.contains("updatedMCPToolOutput")&&
      value::truthy(specific.at("updatedMCPToolOutput")))
      result.fields["updatedMCPToolOutput"]=specific.at("updatedMCPToolOutput");
    return;
  }
  if(event=="PermissionDenied"){
    assignOptional(result,"retry",specific,"retry");return;
  }
  if(event=="PermissionRequest"){
    if(specific.contains("decision")&&value::truthy(specific.at("decision"))){
      const auto& decision=specific.at("decision");
      result.fields["permissionRequestResult"]=decision;
      const auto behavior=fieldString(decision,"behavior");
      result.fields["permissionBehavior"]=behavior=="allow"?"allow":"deny";
      if(behavior=="allow"&&decision.is_object()&&
        decision.contains("updatedInput")&&
        value::truthy(decision.at("updatedInput")))
        result.fields["updatedInput"]=decision.at("updatedInput");
    }
    return;
  }
  if(event=="Elicitation"){
    addElicitation(input,result,specific,"elicitationResponse",
      "Elicitation denied by hook");return;
  }
  if(event=="ElicitationResult")
    addElicitation(input,result,specific,"elicitationResultResponse",
      "Elicitation result blocked by hook");
}
} // namespace batbox::instructions::hooks::runtime::detail
