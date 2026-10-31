#include "application_hooks/Provider.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::application_hooks {
FlightStartOutput Provider::collectFlightStart(
    const std::vector<Json>& events)const{
  if(!owner_.isOwnerThread())throw std::logic_error(
    "SessionStart hook results require owner loop");
  FlightStartOutput output;
  Json additional=Json::array();
  for(const auto& event:events){
    const auto& message=value::property(event,"message");
    if(!message.is_null())output.messages.push_back(message);
    const auto& contexts=value::property(event,"additionalContexts");
    if(contexts.is_array())for(const auto& context:contexts)
      if(context.is_string())additional.push_back(context);
    const auto& initial=value::property(event,"initialUserMessage");
    if(initial.is_string()&&!initial.get<std::string>().empty())
      output.initialUserMessage=initial.get<std::string>();
    const auto& paths=value::property(event,"watchPaths");
    if(paths.is_array())for(const auto& path:paths)
      if(path.is_string())output.watchPaths.push_back(
        path.get<std::string>());
  }
  if(!additional.empty()){
    output.messages.push_back(inputs_.createAttachmentMessage(Json{
      {"type","hook_additional_context"},{"content",additional},
      {"hookName","SessionStart"},{"toolUseID","SessionStart"},
      {"hookEvent","SessionStart"}}));
  }
  return output;
}
} // namespace batbox::application_hooks
