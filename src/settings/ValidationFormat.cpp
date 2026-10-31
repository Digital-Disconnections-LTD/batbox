#include "settings/Validation.hpp"
#include <regex>

namespace batbox::settings {
namespace {
std::string jsString(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_null())return "null";
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_array()){
    std::string result;
    for(const auto& item:value){
      if(!result.empty())result+=',';
      if(!item.is_null())result+=jsString(item);
    }
    return result;
  }
  return "[object Object]";
}
std::string receivedType(const Json& issue){
  if(!issue.is_object()||!issue.contains("input"))return "undefined";
  const auto& value=issue["input"];
  if(value.is_null())return "null";
  if(value.is_array())return "array";
  if(value.is_object())return "object";
  if(value.is_string())return "string";
  if(value.is_boolean())return "boolean";
  if(value.is_number())return "number";
  return "undefined";
}
std::optional<std::string> extractReceived(const std::string& message){
  static const std::regex pattern(R"(received (\w+))");
  std::smatch match;
  if(std::regex_search(message,match,pattern))return match[1].str();
  return std::nullopt;
}
std::string pathString(const Json& path){
  if(!path.is_array())return {};
  std::string result;
  for(const auto& item:path){
    if(!result.empty())result+='.';
    result+=jsString(item);
  }
  return result;
}
}
Json formatZodError(const Json& issues,const std::string& filePath,
  const std::string& docsBase){
  Json result=Json::array();
  if(!issues.is_array())return result;
  for(const auto& issue:issues){
    const auto path=pathString(value::property(issue,"path"));
    const auto& rawMessage=value::property(issue,"message");
    auto message=rawMessage.is_string()?rawMessage.get<std::string>():
      std::string{};
    const auto& rawCode=value::property(issue,"code");
    const auto code=rawCode.is_string()?rawCode.get<std::string>():
      std::string{};
    std::optional<std::string> expected,expectedValue;
    std::optional<Json> invalidValue,received;
    Json enumValues=nullptr;
    if(code=="invalid_value"){
      enumValues=Json::array();
      const auto& values=value::property(issue,"values");
      if(values.is_array())for(const auto& value:values)
        enumValues.push_back(jsString(value));
      std::string joined;
      for(const auto& value:enumValues){
        if(!joined.empty())joined+=" | ";
        joined+=value.get<std::string>();
      }
      expectedValue=joined;
    }else if(code=="invalid_type"){
      const auto& rawExpected=value::property(issue,"expected");
      expectedValue=rawExpected.is_string()?rawExpected.get<std::string>():
        jsString(rawExpected);
      const auto extracted=extractReceived(message);
      received=extracted?Json(*extracted):Json(receivedType(issue));
      invalidValue=received;
    }else if(code=="too_small"){
      const auto& minimum=value::property(issue,"minimum");
      expectedValue=jsString(minimum);
    }else if(code=="custom"&&issue.is_object()&&
             issue.contains("params")){
      const auto& params=issue["params"];
      if(params.is_object()&&params.contains("received")){
        received=params["received"];invalidValue=received;
      }
    }
    Json context={{"path",path},{"code",code},{"message",message}};
    if(expectedValue)context["expected"]=*expectedValue;
    if(received){context["received"]=*received;
      context["value"]=*received;}
    if(enumValues.is_array())context["enumValues"]=enumValues;
    const auto tip=getValidationTip(context,docsBase);
    if(code=="invalid_value"){
      std::string values;
      for(const auto& value:enumValues){
        if(!values.empty())values+=", ";
        values+='"'+value.get<std::string>()+'"';
      }
      expected=values;
      message="Invalid value. Expected one of: "+values;
    }else if(code=="invalid_type"){
      const auto type=extractReceived(message).value_or(receivedType(issue));
      if(expectedValue&&*expectedValue=="object"&&type=="null"&&
         path.empty())message="Invalid or malformed JSON";
      else message="Expected "+expectedValue.value_or("undefined")+
        ", but received "+type;
    }else if(code=="unrecognized_keys"){
      const auto& keys=value::property(issue,"keys");
      std::string joined;
      if(keys.is_array())for(const auto& key:keys){
        if(!joined.empty())joined+=", ";
        joined+=jsString(key);
      }
      const auto count=keys.is_array()?keys.size():0;
      message="Unrecognized "+std::string(count==1?"field":"fields")+
        ": "+joined;
    }else if(code=="too_small"){
      message="Number must be greater than or equal to "+
        expectedValue.value_or("undefined");
      expected=expectedValue;
    }
    Json formatted={{"file",filePath},{"path",path},{"message",message}};
    if(expected)formatted["expected"]=*expected;
    if(invalidValue)formatted["invalidValue"]=*invalidValue;
    if(tip.suggestion)formatted["suggestion"]=*tip.suggestion;
    if(tip.docLink)formatted["docLink"]=*tip.docLink;
    result.push_back(std::move(formatted));
  }
  return result;
}
}
