#include "instructions/hooks/runtime/HookSchema.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
bool optionalField(const Json& input,Json& output,const char* name,
  const std::string& type){
  if(!input.contains(name))return true;
  const auto& value=input.at(name);
  const bool valid=type=="string"?value.is_string():
    type=="boolean"?value.is_boolean():
    type=="number"?value.is_number():
    type=="object"?value.is_object():false;
  if(!valid)return false;
  output[name]=value;return true;
}
bool optionalEnum(const Json& input,Json& output,const char* name,
  std::initializer_list<const char*> options){
  if(!input.contains(name))return true;
  if(!input.at(name).is_string())return false;
  const auto value=input.at(name).get<std::string>();
  for(const auto* option:options)if(value==option){output[name]=value;return true;}
  return false;
}
bool optionalStrings(const Json& input,Json& output,const char* name){
  if(!input.contains(name))return true;
  const auto& value=input.at(name);
  if(!value.is_array())return false;
  for(const auto& item:value)if(!item.is_string())return false;
  output[name]=value;return true;
}
bool parsePermissionDecision(const Json& input,Json& output,
  const HookSchemaImports& imports){
  if(!input.is_object()||!input.contains("behavior")||
    !input.at("behavior").is_string())return false;
  const auto behavior=input.at("behavior").get<std::string>();
  if(behavior=="allow"){
    output["behavior"]="allow";
    if(!optionalField(input,output,"updatedInput","object"))return false;
    if(input.contains("updatedPermissions")){
      if(!input.at("updatedPermissions").is_array())return false;
      if(!imports.parsePermissionUpdate)
        throw std::invalid_argument("Permission update schema import required");
      Json values=Json::array();
      for(const auto& item:input.at("updatedPermissions")){
        const auto parsed=imports.parsePermissionUpdate(item);
        if(!parsed)return false;
        values.push_back(*parsed);
      }
      output["updatedPermissions"]=std::move(values);
    }
    return true;
  }
  if(behavior=="deny"){
    output["behavior"]="deny";
    return optionalField(input,output,"message","string")&&
      optionalField(input,output,"interrupt","boolean");
  }
  return false;
}
bool parseSpecific(const Json& input,Json& output,
  const HookSchemaImports& imports){
  if(!input.is_object()||!input.contains("hookEventName")||
    !input.at("hookEventName").is_string())return false;
  const auto event=input.at("hookEventName").get<std::string>();
  output["hookEventName"]=event;
  if(event=="PreToolUse"){
    return optionalEnum(input,output,"permissionDecision",{"allow","deny","ask"})&&
      optionalField(input,output,"permissionDecisionReason","string")&&
      optionalField(input,output,"updatedInput","object")&&
      optionalField(input,output,"additionalContext","string");
  }
  if(event=="UserPromptSubmit"||event=="Setup"||
    event=="SubagentStart"||event=="PostToolUseFailure"||
    event=="Notification")
    return optionalField(input,output,"additionalContext","string");
  if(event=="SessionStart")
    return optionalField(input,output,"additionalContext","string")&&
      optionalField(input,output,"initialUserMessage","string")&&
      optionalStrings(input,output,"watchPaths");
  if(event=="PostToolUse"){
    if(!optionalField(input,output,"additionalContext","string"))return false;
    if(input.contains("updatedMCPToolOutput"))
      output["updatedMCPToolOutput"]=input.at("updatedMCPToolOutput");
    return true;
  }
  if(event=="PermissionDenied")
    return optionalField(input,output,"retry","boolean");
  if(event=="PermissionRequest"){
    if(!input.contains("decision"))return true;
    Json decision=Json::object();
    if(!parsePermissionDecision(input.at("decision"),decision,imports))return false;
    output["decision"]=std::move(decision);return true;
  }
  if(event=="Elicitation"||event=="ElicitationResult")
    return optionalEnum(input,output,"action",{"accept","decline","cancel"})&&
      optionalField(input,output,"content","object");
  if(event=="CwdChanged"||event=="FileChanged")
    return optionalStrings(input,output,"watchPaths");
  if(event=="WorktreeCreate"){
    if(!input.contains("worktreePath")||
      !input.at("worktreePath").is_string())return false;
    output["worktreePath"]=input.at("worktreePath");return true;
  }
  return false;
}
bool parseAsync(const Json& input,Json& output){
  if(!input.is_object()||!input.contains("async")||input.at("async")!=true)
    return false;
  output["async"]=true;
  return optionalField(input,output,"asyncTimeout","number");
}
bool parseSync(const Json& input,Json& output,
  const HookSchemaImports& imports){
  if(!input.is_object())return false;
  if(!optionalField(input,output,"continue","boolean")||
    !optionalField(input,output,"suppressOutput","boolean")||
    !optionalField(input,output,"stopReason","string")||
    !optionalEnum(input,output,"decision",{"approve","block"})||
    !optionalField(input,output,"reason","string")||
    !optionalField(input,output,"systemMessage","string"))return false;
  if(input.contains("hookSpecificOutput")){
    Json specific=Json::object();
    if(!parseSpecific(input.at("hookSpecificOutput"),specific,imports))return false;
    output["hookSpecificOutput"]=std::move(specific);
  }
  return true;
}
}
SchemaResult hookOutputSchemaSafeParse(const Json& input,
  const HookSchemaImports& imports){
  Json output=Json::object();
  if(parseAsync(input,output))return {output,{}};
  output=Json::object();
  if(parseSync(input,output,imports))return {output,{}};
  // Zod v4's outer z.union reports the root invalid_union message after both
  // object branches fail; validateHookJson prints only path and message.
  return {std::nullopt,{{{},"Invalid input"}}};
}
void bindHookOutputSchema(HookParserServices& parser,HookSchemaImports imports){
  parser.schemaSafeParse=[imports=std::move(imports)](const Json& value){
    return hookOutputSchemaSafeParse(value,imports);
  };
}
} // namespace batbox::instructions::hooks::runtime
