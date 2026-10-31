#include "cli/structured/IndigenousSchemas.hpp"
#include "instructions/hooks/runtime/HookSchema.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>
#include <string_view>

namespace batbox::cli::structured {
namespace {
bool oneOf(const Json& value,std::initializer_list<std::string_view> names){
  if(!value.is_string())return false;
  const auto text=value.get<std::string>();
  for(const auto option:names)if(text==option)return true;
  return false;
}
bool record(const Json& value){return value.is_object();}
bool copyString(const Json& input,Json& output,const char* name,bool required){
  if(!input.contains(name))return !required;
  const auto& field=input.at(name);
  if(!field.is_string())return false;
  output[name]=field;return true;
}
bool copyBool(const Json& input,Json& output,const char* name){
  if(!input.contains(name))return true;
  const auto& field=input.at(name);
  if(!field.is_boolean())return false;
  output[name]=field;return true;
}
bool copyRuleValues(const Json& input,Json& output){
  const auto& rules=value::property(input,"rules");
  if(!rules.is_array())return false;
  Json parsed=Json::array();
  for(const auto& rule:rules){
    if(!record(rule))return false;
    Json item=Json::object();
    if(!copyString(rule,item,"toolName",true)||
       !copyString(rule,item,"ruleContent",false))return false;
    parsed.push_back(std::move(item));
  }
  output["rules"]=std::move(parsed);return true;
}
bool copyDirectoryValues(const Json& input,Json& output){
  const auto& directories=value::property(input,"directories");
  if(!directories.is_array())return false;
  for(const auto& item:directories)if(!item.is_string())return false;
  output["directories"]=directories;return true;
}
void requireObject(const Json& value){
  if(!record(value))throw std::invalid_argument("Invalid input");
}
}
std::optional<Json> parsePermissionUpdate(const Json& value){
  if(!record(value))return {};
  const auto& type=value::property(value,"type");
  const auto& destination=value::property(value,"destination");
  if(!oneOf(destination,{"userSettings","projectSettings","localSettings",
                         "session","cliArg"}))return {};
  Json output={{"type",type},{"destination",destination}};
  if(oneOf(type,{"addRules","replaceRules","removeRules"})){
    const auto& behavior=value::property(value,"behavior");
    if(!oneOf(behavior,{"allow","deny","ask"})||
       !copyRuleValues(value,output))return {};
    output["behavior"]=behavior;return output;
  }
  if(type=="setMode"){
    const auto& mode=value::property(value,"mode");
    if(!oneOf(mode,{"acceptEdits","bypassPermissions","default",
                    "dontAsk","plan"}))return {};
    output["mode"]=mode;return output;
  }
  if(oneOf(type,{"addDirectories","removeDirectories"})&&
     copyDirectoryValues(value,output))return output;
  return {};
}
Json parsePermissionOutput(const Json& value,
    const IndigenousSchemaServices& services){
  requireObject(value);
  const auto& behavior=value::property(value,"behavior");
  Json output=Json::object();
  if(behavior=="allow"){
    const auto& updated=value::property(value,"updatedInput");
    if(!record(updated))throw std::invalid_argument("Invalid input");
    output["behavior"]="allow";
    output["updatedInput"]=updated;
    if(value.contains("updatedPermissions")){
      const auto& changes=value.at("updatedPermissions");
      Json parsed=Json::array();bool valid=changes.is_array();
      if(valid)for(const auto& change:changes){
        const auto item=parsePermissionUpdate(change);
        if(!item){valid=false;break;}
        parsed.push_back(*item);
      }
      if(valid)output["updatedPermissions"]=std::move(parsed);
      else{
        if(!services.logMalformedPermissionUpdates)
          throw std::invalid_argument("Malformed permission log import required");
        services.logMalformedPermissionUpdates(
          "Malformed updatedPermissions from SDK host ignored: Invalid input");
      }
    }
  }else if(behavior=="deny"){
    output["behavior"]="deny";
    if(!copyString(value,output,"message",true)||
       !copyBool(value,output,"interrupt"))
      throw std::invalid_argument("Invalid input");
  }else throw std::invalid_argument("Invalid input");
  if(!copyString(value,output,"toolUseID",false))
    throw std::invalid_argument("Invalid input");
  const auto& classification=value::property(value,"decisionClassification");
  if(oneOf(classification,{"user_temporary","user_permanent","user_reject"}))
    output["decisionClassification"]=classification;
  return output;
}
Json parseElicitationResponse(const Json& value){
  requireObject(value);
  const auto& action=value::property(value,"action");
  if(!oneOf(action,{"accept","decline","cancel"}))
    throw std::invalid_argument("Invalid input");
  Json output={{"action",action}};
  if(value.contains("content")){
    if(!record(value.at("content")))
      throw std::invalid_argument("Invalid input");
    output["content"]=value.at("content");
  }
  return output;
}
Json parseHookOutput(const Json& value){
  instructions::hooks::runtime::HookSchemaImports imports;
  imports.parsePermissionUpdate=parsePermissionUpdate;
  const auto result=instructions::hooks::runtime::hookOutputSchemaSafeParse(
    value,imports);
  if(!result.json)throw std::invalid_argument("Invalid input");
  return *result.json;
}
Json parseMcpResponse(const Json& value){
  requireObject(value);
  // Zod v4 z.any() accepts an absent key as undefined. JSON omits it.
  if(!value.contains("mcp_response"))return Json::object();
  return Json{{"mcp_response",value.at("mcp_response")}};
}
ControlSchemas makeIndigenousControlSchemas(IndigenousSchemaServices services){
  ControlSchemas schemas;
  schemas.hookOutput=parseHookOutput;
  schemas.elicitationResponse=parseElicitationResponse;
  schemas.permissionOutput=[services=std::move(services)](const Json& value){
    return parsePermissionOutput(value,services);
  };
  schemas.mcpResponse=parseMcpResponse;
  return schemas;
}
}
