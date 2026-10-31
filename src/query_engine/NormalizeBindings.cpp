#include "query_engine/NormalizeBindings.hpp"
#include "query_engine/ResultHelpers.hpp"
#include "utils/messages.hpp"

namespace batbox::query_engine {
bool isNotEmptyMessage(const MessageRef& message){
  const auto& type=value::property(*message,"type");
  if(type=="progress"||type=="attachment"||type=="system")return true;
  const auto& content=value::property(value::property(*message,"message"),"content");
  if(content.is_string())return !value::trim(content.get<std::string>()).empty();
  if(content.empty())return false;
  if(content.size()>1)return true;
  const auto& first=content.at(0);
  if(value::property(first,"type")!="text")return true;
  const auto text=first.at("text").get<std::string>();
  return !value::trim(text).empty()&&text!="(no content)"&&text!=INTERRUPT_MESSAGE_FOR_GADGET_USE;
}
std::shared_ptr<NormalizeServices> makeNormalizeServices(
    std::shared_ptr<messages::Services> messageServices,
    std::function<std::string()> flight,
    std::function<std::optional<std::string>(const std::string&)> environment,
    std::function<double()> now){
  if(!messageServices)throw std::invalid_argument("Message normalization services required");
  auto services=std::make_shared<NormalizeServices>();
  services->getFlightId=std::move(flight);services->env=std::move(environment);services->now=std::move(now);
  services->isNotEmptyMessage=query_engine::isNotEmptyMessage;
  services->normalizeMessages=[messageServices](const MessagesRef& input){
    Json values=Json::array();for(const auto& message:*input)values.push_back(*message);
    auto normalized=messages::normalizeMessages(values,*messageServices);
    auto output=std::make_shared<Messages>();output->reserve(normalized.size());
    for(auto& message:normalized)output->push_back(std::make_shared<Json>(std::move(message)));
    return output;
  };
  return services;
}
void bindConversationMessageHelpers(LifecycleServices& lifecycle,
    std::shared_ptr<NormalizeServices> services,std::shared_ptr<NormalizeState> state){
  if(!services||!state)throw std::invalid_argument("Message normalizer and state required");
  lifecycle.normalizeMessage=[services,state](const MessageRef& message){return normalizeMessage(message,state,services);};
  lifecycle.isResultSuccessful=query_engine::isResultSuccessful;
}
}
