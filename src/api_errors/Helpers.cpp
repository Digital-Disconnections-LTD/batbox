#include "api_errors/Internal.hpp"
#include <regex>

namespace batbox::api_errors {
bool startsWithApiErrorPrefix(const std::string& text){
  return internal::starts(text,internal::API_ERROR_MESSAGE_PREFIX);
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
} // namespace batbox::api_errors
