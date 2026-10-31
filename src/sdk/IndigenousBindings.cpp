#include "sdk/IndigenousBindings.hpp"
#include "runtime/Platform.hpp"
#include "utils/envUtils.hpp"
#include <cstdlib>
#include <stdexcept>
namespace batbox::sdk {
QueueServices indigenousQueueServices(std::function<bool()> nonInteractive,
    std::function<std::string()> flightId){
  if(!nonInteractive||!flightId)throw std::invalid_argument("SDK session state providers required");
  return QueueServices{std::move(nonInteractive),platform::randomUUID,std::move(flightId)};
}
flights::LifecycleServices lifecycleQueueServices(std::shared_ptr<EventQueue> queue,
    std::function<std::optional<std::string>(const std::string&)> environment){
  if(!queue)throw std::invalid_argument("SDK event queue required");
  if(!environment)environment=[](const std::string& name)->std::optional<std::string>{
    if(const auto* value=std::getenv(name.c_str()))return std::string(value);return {};
  };
  return {[environment=std::move(environment)]{
      const auto value=environment("BATBOX_EMIT_SESSION_STATE_EVENTS");
      return isEnvTruthy(value?Json(*value):Json());
    },[queue=std::move(queue)](value::RecordRef event){queue->enqueueSdkEvent(std::move(event));}};
}
void bindBatSdkQueue(bats::BatRegistrationServices& services,std::shared_ptr<EventQueue> queue){
  if(!queue)throw std::invalid_argument("SDK event queue required");
  services.enqueueSdkEvent=[queue=std::move(queue)](const Json& event){
    queue->enqueueSdkEvent(std::make_shared<value::SharedRecord>(value::SharedRecord::fromJson(event)));
  };
}
void bindForegroundSdkQueue(bats::BatGadgetForegroundFinalServices& services,std::shared_ptr<EventQueue> queue){
  if(!queue)throw std::invalid_argument("SDK event queue required");
  services.enqueueSdkEvent=[queue=std::move(queue)](const Json& event){
    queue->enqueueSdkEvent(std::make_shared<value::SharedRecord>(value::SharedRecord::fromJson(event)));
  };
}
} // namespace batbox::sdk
