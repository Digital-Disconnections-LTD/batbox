#include "instructions/hooks/runtime/HookOutputParser.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
void debug(const HookParserServices& services,const std::string& message){
  if(!services.logDebug)throw std::invalid_argument("Hook parser debug import required");
  services.logDebug(message);
}
std::string thrown(const HookParserServices& services,std::exception_ptr error){
  if(!services.stringifyThrown)
    throw std::invalid_argument("Hook parser exception formatter required");
  return services.stringifyThrown(error);
}
Json expectedSchema(){
  Json pre={{"hookEventName","\"PreToolUse\""},
    {"permissionDecision","\"allow\" | \"deny\" | \"ask\" (optional)"},
    {"permissionDecisionReason","string (optional)"},
    {"updatedInput","object (optional) - Modified tool input to use"}};
  Json user={{"hookEventName","\"UserPromptSubmit\""},
    {"additionalContext","string (required)"}};
  Json post={{"hookEventName","\"PostToolUse\""},
    {"additionalContext","string (optional)"}};
  Json specific={{"for PreToolUse",pre},{"for UserPromptSubmit",user},
    {"for PostToolUse",post}};
  return Json{{"continue","boolean (optional)"},
    {"suppressOutput","boolean (optional)"},
    {"stopReason","string (optional)"},
    {"decision","\"approve\" | \"block\" (optional)"},
    {"reason","string (optional)"},
    {"systemMessage","string (optional)"},
    {"permissionDecision","\"allow\" | \"deny\" | \"ask\" (optional)"},
    {"hookSpecificOutput",specific}};
}
}
HookParseResult validateHookJson(const std::string& jsonString,
  const HookParserServices& services){
  if(!services.schemaSafeParse)
    throw std::invalid_argument("Hook JSON schema import required");
  const auto parsed=value::parse(jsonString);
  const auto validation=services.schemaSafeParse(parsed);
  if(validation.json){
    debug(services,"Successfully parsed and validated hook JSON output");
    return {validation.json,std::nullopt,std::nullopt};
  }
  std::string errors;
  for(const auto& issue:validation.issues){
    if(!errors.empty())errors+='\n';
    errors+="  - ";
    for(std::size_t i=0;i<issue.path.size();++i){
      if(i)errors+='.';
      errors+=issue.path[i];
    }
    errors+=": "+issue.message;
  }
  return {std::nullopt,std::nullopt,
    "Hook JSON output validation failed:\n"+errors+
      "\n\nThe hook's output was: "+value::stringify(parsed,2)};
}
HookParseResult parseHookOutput(const std::string& stdoutText,
  const HookParserServices& services){
  const auto trimmed=value::trim(stdoutText);
  if(!trimmed.starts_with('{')){
    debug(services,"Hook output does not start with {, treating as plain text");
    return {std::nullopt,stdoutText,std::nullopt};
  }
  try{
    auto result=validateHookJson(trimmed,services);
    if(result.json)return result;
    const auto error=*result.validationError+"\n\nExpected schema:\n"+
      value::stringify(expectedSchema(),2);
    debug(services,error);
    return {std::nullopt,stdoutText,error};
  }catch(...){
    debug(services,"Failed to parse hook output as JSON: "+
      thrown(services,std::current_exception()));
    return {std::nullopt,stdoutText,std::nullopt};
  }
}
HookParseResult parseHttpHookOutput(const std::string& body,
  const HookParserServices& services){
  const auto trimmed=value::trim(body);
  if(trimmed.empty()){
    if(!services.schemaSafeParse)
      throw std::invalid_argument("Hook JSON schema import required");
    const auto validation=services.schemaSafeParse(Json::object());
    if(validation.json){
      debug(services,"HTTP hook returned empty body, treating as empty JSON object");
      return {validation.json,std::nullopt,std::nullopt};
    }
  }
  if(!trimmed.starts_with('{')){
    const auto error="HTTP hook must return JSON, but got non-JSON response body: "+
      (value::stringLength(trimmed)>200?value::slice(trimmed,0,200)+"…":trimmed);
    debug(services,error);
    return {std::nullopt,std::nullopt,error};
  }
  try{
    auto result=validateHookJson(trimmed,services);
    if(result.json)return result;
    debug(services,*result.validationError);
    return result;
  }catch(...){
    const auto error="HTTP hook must return valid JSON, but parsing failed: "+
      thrown(services,std::current_exception());
    debug(services,error);
    return {std::nullopt,std::nullopt,error};
  }
}
void bindHookOutputParsers(ExecutorServices& executor,HookParserServices services){
  executor.parseHookOutput=[services](const std::string& text){
    const auto result=parseHookOutput(text,services);
    return ParsedOutput{result.json,result.validationError};
  };
  executor.parseHttpHookOutput=[services](const std::string& body){
    const auto result=parseHttpHookOutput(body,services);
    return ParsedOutput{result.json,result.validationError};
  };
}
} // namespace batbox::instructions::hooks::runtime
