#include "application_model/Internal.hpp"
#include "normalization/Normalization.hpp"
#include <stdexcept>

namespace batbox::application_model {
namespace {
void requireObject(const Json& context){
  if(!context.is_object())throw std::invalid_argument("Context must be an object");
}
std::string contextValue(const Json& value){
  if(!value.is_string())throw std::invalid_argument("Context values must be strings");
  return value.get<std::string>();
}
}
Json LocalModelService::normalizeMessagesForAPI(const Json& source,
    const gadgets::Gadgets& active){
  LocalServiceBuilder builder(*this,active,Json::object());
  return normalization::normalizeMessagesForAPI(source,builder.gadgetMetadata(),
    builder.normalization);
}
Json LocalModelService::appendSystemContext(const Json& systemPoetry,
    const Json& context)const{
  requireObject(context);
  if(!systemPoetry.is_array())
    throw std::invalid_argument("System prompt must be an array");
  Json result=Json::array();
  for(const auto& block:systemPoetry)
    if(value::truthy(block))result.push_back(block);
  std::string joined;
  for(auto it=context.begin();it!=context.end();++it){
    if(!joined.empty())joined+='\n';
    joined+=it.key()+": "+contextValue(it.value());
  }
  if(!joined.empty())result.push_back(std::move(joined));
  return result;
}
Json LocalModelService::prependUserContext(const Json& source,
    const Json& context){
  requireObject(context);
  if(!source.is_array())throw std::invalid_argument("Messages must be an array");
  if(config_.testEnvironment||context.empty())return source;
  std::string entries;
  for(auto it=context.begin();it!=context.end();++it){
    if(!entries.empty())entries+='\n';
    entries+="# "+it.key()+"\n"+contextValue(it.value());
  }
  std::string content="<system-reminder>\nAs you answer the user's questions, you can use the following context:\n"+
    entries+"\n\n      IMPORTANT: this context may or may not be relevant to your tasks. You should not respond to this context unless it is highly relevant to your task.\n</system-reminder>\n";
  auto services=LocalServiceBuilder(*this,{},Json::object()).makeMessages();
  Json result=Json::array();
  result.push_back(messages::createUserMessage(
    Json{{"content",std::move(content)},{"isMeta",true}},services));
  for(const auto& message:source)result.push_back(message);
  return result;
}
} // namespace batbox::application_model
