#include "settings/schema/Internal.hpp"

namespace batbox::settings::schema::detail {
std::string typeName(const Json& value){
  if(value.is_null())return "null";
  if(value.is_array())return "array";
  if(value.is_object())return "object";
  if(value.is_string())return "string";
  if(value.is_boolean())return "boolean";
  if(value.is_number())return "number";
  return "undefined";
}
std::string jsString(const Json& value){
  if(value.is_null())return "null";
  if(value.is_string())return value.get<std::string>();
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_array()){
    std::string result;
    bool first=true;
    for(const auto& item:value){
      if(!first)result+=',';
      if(!item.is_null())result+=jsString(item);
      first=false;
    }
    return result;
  }
  return "[object Object]";
}
void invalidType(Json& issues,const Json& path,
  const std::string& expected,const Json& input){
  issues.push_back({{"expected",expected},{"code","invalid_type"},
    {"path",path},{"message","Invalid input: expected "+expected+
      ", received "+typeName(input)}});
}
void invalidValue(Json& issues,const Json& path,
  const std::vector<std::string>& values){
  Json names=Json::array();
  std::string message;
  for(const auto& value:values){
    names.push_back(value);
    if(!message.empty())message+='|';
    message+='"'+value+'"';
  }
  message=values.size()==1?"Invalid input: expected "+message:
    "Invalid option: expected one of "+message;
  issues.push_back({{"code","invalid_value"},{"values",names},
    {"path",path},{"message",message}});
}
bool stringField(const Json& input,const char* key,Json& output,
  Json& issues){
  if(!input.contains(key))return false;
  const auto& value=input[key];
  if(!value.is_string()){
    invalidType(issues,Json::array({key}),"string",value);return false;
  }
  output[key]=value;
  return true;
}
bool stringArrayField(const Json& input,const char* key,Json& output,
  Json& issues){
  if(!input.contains(key))return false;
  const auto& value=input[key];
  if(!value.is_array()){
    invalidType(issues,Json::array({key}),"array",value);return false;
  }
  Json parsed=Json::array();
  bool valid=true;
  for(std::size_t i=0;i<value.size();++i){
    if(value[i].is_string())parsed.push_back(value[i]);
    else{
      invalidType(issues,Json::array({key,i}),"string",value[i]);
      valid=false;
    }
  }
  if(valid)output[key]=parsed;
  return valid;
}
}
