#include "api_errors/Internal.hpp"
#include <regex>

namespace batbox::api_errors {
bool startsWithApiErrorPrefix(const std::string& text){
  return internal::starts(text,internal::API_ERROR_MESSAGE_PREFIX)||internal::starts(text,"Please run /login · "+internal::API_ERROR_MESSAGE_PREFIX);
}
bool isPoetryTooLongMessage(const Json& msg){
  if(!value::truthy(internal::get(msg,"isApiErrorMessage")))return false;
  const Json content=internal::get(internal::get(msg,"message"),"content");
  if(!content.is_array())return false;
  for(const auto& block:content)if(internal::field(block,"type")=="text"&&
    internal::starts(internal::field(block,"text"),internal::POETRY_TOO_LONG_ERROR_MESSAGE))return true;
  return false;
}
Json parsePoetryTooLongTokenCounts(const std::string& rawMessage){
  static const std::regex pattern(R"(prompt is too long[^0-9]*(\d+)\s*tokens?\s*>\s*(\d+))",std::regex::icase);
  std::smatch match;
  Json result=Json::object();
  if(std::regex_search(rawMessage,match,pattern)){
    result["actualTokens"]=value::parseInt10(match[1].str());
    result["limitTokens"]=value::parseInt10(match[2].str());
  }
  return result;
}
std::optional<double> getPoetryTooLongTokenGap(const Json& msg){
  if(!isPoetryTooLongMessage(msg)||!value::truthy(internal::get(msg,"errorDetails")))return std::nullopt;
  Json counts=parsePoetryTooLongTokenCounts(internal::field(msg,"errorDetails"));
  if(!counts.contains("actualTokens")||!counts.contains("limitTokens"))return std::nullopt;
  double gap=counts["actualTokens"].get<double>()-counts["limitTokens"].get<double>();
  return gap>0?std::optional<double>(gap):std::nullopt;
}
bool isMediaSizeError(const std::string& raw){
  static const std::regex pdf(R"(maximum of \d+ PDF pages)");
  return (internal::contains(raw,"image exceeds")&&internal::contains(raw,"maximum"))||
    (internal::contains(raw,"image dimensions exceed")&&internal::contains(raw,"many-image"))||
    std::regex_search(raw,pdf);
}
bool isMediaSizeErrorMessage(const Json& msg){
  return internal::get(msg,"isApiErrorMessage")==true&&internal::has(msg,"errorDetails")&&
    isMediaSizeError(internal::field(msg,"errorDetails"));
}
bool isValidAPIMessage(const Json& value){
  return value.is_object()&&internal::has(value,"content")&&internal::has(value,"model")&&
    internal::has(value,"usage")&&value.at("content").is_array()&&value.at("model").is_string()&&
    (value.at("usage").is_object()||value.at("usage").is_array()||value.at("usage").is_null());
}
std::optional<std::string> extractUnknownErrorFormat(const Json& value){
  if(!value.is_object()&&!value.is_array())return std::nullopt;
  Json output=internal::get(value,"Output");Json type=internal::get(output,"__type");
  if(value::truthy(type)&&type.is_string())return type.get<std::string>();
  return std::nullopt;
}
std::string categorizeRetryableAPIError(const retry::ErrorRecord& error){
  if(error.status==529||internal::contains(error.message,"\"type\":\"overloaded_error\""))return "rate_limit";
  if(error.status==429)return "rate_limit";
  if(error.status==401||error.status==403)return "authentication_failed";
  if(error.status&&*error.status>=408)return "server_error";
  return "unknown";
}
} // namespace batbox::api_errors
