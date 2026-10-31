#include "instructions/hooks/runtime/ElicitationHooks.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
std::string field(const Json& value,const char* name){
  if(!value.is_object()||!value.contains(name)||
    !value.at(name).is_string())return "";
  return value.at(name).get<std::string>();
}
}
ElicitationParsed parseElicitationHookOutput(const HookResult& result,
  const std::string& expectedEventName,
  const std::function<SchemaResult(const Json&)>& schemaSafeParse){
  if(result.blocked&&!result.succeeded)
    return {std::nullopt,HookBlockingError{
      result.output.empty()?"Elicitation blocked by hook":result.output,
      result.command}};
  const auto trimmed=value::trim(result.output);
  if(trimmed.empty()||!trimmed.starts_with('{'))return {};
  if(!schemaSafeParse)
    throw std::invalid_argument("Elicitation hook schema import required");
  try{
    const auto validation=schemaSafeParse(value::parse(trimmed));
    if(!validation.json)return {};
    const auto& parsed=*validation.json;
    if(isAsyncHookJsonOutput(parsed))return {};
    if(!isSyncHookJsonOutput(parsed))return {};
    if(field(parsed,"decision")=="block"||result.blocked)
      return {std::nullopt,HookBlockingError{
        field(parsed,"reason").empty()?"Elicitation blocked by hook":
          field(parsed,"reason"),result.command}};
    if(!parsed.is_object()||!parsed.contains("hookSpecificOutput"))return {};
    const auto& specific=parsed.at("hookSpecificOutput");
    if(field(specific,"hookEventName")!=expectedEventName)return {};
    const auto action=field(specific,"action");
    if(action.empty())return {};
    ElicitationResponse response{action};
    if(specific.contains("content"))response.content=specific.at("content");
    ElicitationParsed output;output.response=std::move(response);
    if(action=="decline"){
      const auto reason=field(parsed,"reason");
      output.blockingError=HookBlockingError{reason.empty()
        ?(expectedEventName=="Elicitation"?
          "Elicitation denied by hook":"Elicitation result blocked by hook")
        :reason,result.command};
    }
    return output;
  }catch(...){return {};}
}
} // namespace batbox::instructions::hooks::runtime
