#include "gadgets/web/Web.hpp"
#include <stdexcept>

namespace batbox::gadgets::web {
namespace {
void requireObject(const Json& input,const Json& schema){
  if(!input.is_object())throw std::invalid_argument("Expected tool input object");
  const auto& properties=schema.at("properties");
  for(auto it=input.begin();it!=input.end();++it)
    if(!properties.contains(it.key()))throw std::invalid_argument("Unrecognized web tool input key: "+it.key());
}
void requireString(const Json& input,const char* key){
  if(!input.contains(key)||!input.at(key).is_string())
    throw std::invalid_argument(std::string("Expected string web tool input: ")+key);
}
}
Json parseWebSearchInput(const Json& input){
  requireObject(input,webSearchInputSchema());
  requireString(input,"query");
  if(value::stringLength(input.at("query").get<std::string>())<2)
    throw std::invalid_argument("Search query must contain at least two characters");
  for(const char* key:{"allowed_domains","blocked_domains"})if(input.contains(key)){
    if(!input.at(key).is_array())throw std::invalid_argument(std::string("Expected array: ")+key);
    for(const auto& domain:input.at(key))if(!domain.is_string())
      throw std::invalid_argument(std::string("Expected string domain: ")+key);
  }
  return input;
}
Json parseWebFetchInput(const Json& input){
  requireObject(input,webFetchInputSchema());
  requireString(input,"url");requireString(input,"prompt");
  if(!hostnameFromURL(input.at("url").get<std::string>()))
    throw std::invalid_argument("Invalid web fetch URL");
  return input;
}
} // namespace batbox::gadgets::web
