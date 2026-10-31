#include "settings/remote/ResponseSchema.hpp"
#include <array>

namespace batbox::settings::remote {
namespace {
std::string typeName(const Json& value){
  if(value.is_null())return "null";
  if(value.is_string())return "string";
  if(value.is_boolean())return "boolean";
  if(value.is_number())return "number";
  if(value.is_array())return "array";
  return "object";
}
void invalid(Json& issues,const std::string& expected,
    const std::string& received,const Json& path){
  issues.push_back(Json{{"expected",expected},{"code","invalid_type"},
    {"path",path},{"message","Invalid input: expected "+expected+
      ", received "+received}});
}
}
ResponseParseResult parseRemoteManagedSettingsResponse(const Json& input){
  Json issues=Json::array(),data=Json::object();
  if(!input.is_object()){
    invalid(issues,"object",typeName(input),Json::array());
    return {false,nullptr,value::stringify(issues,2)};
  }
  for(const char* field:std::array{"uuid","checksum"}){
    const Json path=Json::array({field});
    if(!input.contains(field))invalid(issues,"string","undefined",path);
    else if(!input[field].is_string())
      invalid(issues,"string",typeName(input[field]),path);
    else data[field]=input[field];
  }
  if(!input.contains("settings"))
    invalid(issues,"record","undefined",Json::array({"settings"}));
  else if(!input["settings"].is_object())
    invalid(issues,"record",typeName(input["settings"]),
      Json::array({"settings"}));
  else data["settings"]=input["settings"];
  if(!issues.empty())return {false,nullptr,value::stringify(issues,2)};
  return {true,data,{}};
}
}
