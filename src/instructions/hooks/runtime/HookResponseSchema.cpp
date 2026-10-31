#include "instructions/hooks/runtime/HookResponseSchema.hpp"
#include "instructions/hooks/runtime/BatHookExecutor.hpp"

namespace batbox::instructions::hooks::runtime {
namespace {
std::string receivedType(const Json& value){
  if(value.is_null())return "null";
  if(value.is_boolean())return "boolean";
  if(value.is_number())return "number";
  if(value.is_string())return "string";
  if(value.is_array())return "array";
  if(value.is_object())return "object";
  return "undefined";
}
void invalidType(Json& issues,const char* expected,const Json& path,
  const std::string& received){
  issues.push_back(Json{{"expected",expected},{"code","invalid_type"},
    {"path",path},{"message","Invalid input: expected "+
      std::string(expected)+", received "+received}});
}
}
HookResponseParse hookResponseSchemaSafeParse(const Json& input){
  Json issues=Json::array();
  Json output=Json::object();
  if(!input.is_object()){
    invalidType(issues,"object",Json::array(),receivedType(input));
  }else{
    if(!input.contains("ok"))
      invalidType(issues,"boolean",Json::array({"ok"}),"undefined");
    else if(!input.at("ok").is_boolean())
      invalidType(issues,"boolean",Json::array({"ok"}),
        receivedType(input.at("ok")));
    else output["ok"]=input.at("ok");
    if(input.contains("reason")){
      if(!input.at("reason").is_string())
        invalidType(issues,"string",Json::array({"reason"}),
          receivedType(input.at("reason")));
      else output["reason"]=input.at("reason");
    }
  }
  if(!issues.empty())return {false,Json::object(),issues.dump(2)};
  return {true,std::move(output),{}};
}
void bindHookResponseSchema(PoetryHookServices& poetry,
  BatHookServices& bat){
  poetry.schemaSafeParse=hookResponseSchemaSafeParse;
  bat.schemaSafeParse=hookResponseSchemaSafeParse;
}
} // namespace batbox::instructions::hooks::runtime
