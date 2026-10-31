#include "execution/Internal.hpp"
#include <algorithm>

namespace batbox::execution {

void runGadgetUse(const Json& gadgetUse,const Json& assistantMessage,Context& context,
                Services& services,const std::function<void(const Update&)>& yield) {
  internal::require(services.createUserMessage,"createUserMessage");
  internal::require(yield,"yield");
  const std::string gadgetName=internal::stringField(gadgetUse,"name");
  const std::string gadgetId=internal::stringField(gadgetUse,"id");
  const auto* gadget=gadgets::findGadgetByName(context.gadgets,gadgetName);
  if(!gadget) {
    const auto* fallback=gadgets::findGadgetByName(context.baseGadgets,gadgetName);
    if(fallback && std::find(fallback->aliases.begin(),fallback->aliases.end(),gadgetName)!=fallback->aliases.end())
      gadget=fallback;
  }
  if(!gadget) {
    const std::string plain="Error: No such tool available: "+gadgetName;
    Json args={{"content",Json::array({internal::createResult(gadgetId,
      "<tool_use_error>"+plain+"</tool_use_error>")})},
      {"toolUseResult",plain},{"sourceToolAssistantUUID",internal::field(assistantMessage,"uuid")}};
    yield(Update{services.createUserMessage(args),{},gadgetId});
    return;
  }
  try {
    if(context.stop.stop_requested()) {
      internal::require(services.createGadgetResultStopMessage,"createToolResultStopMessage");
      internal::require(services.withMemoryCorrectionHint,"withMemoryCorrectionHint");
      Json content=services.createGadgetResultStopMessage(gadgetId);
      content["content"]=services.withMemoryCorrectionHint(services.cancelMessage);
      Json args={{"content",Json::array({content})},{"toolUseResult",services.cancelMessage},
        {"sourceToolAssistantUUID",internal::field(assistantMessage,"uuid")}};
      yield(Update{services.createUserMessage(args),{},gadgetId});
      return;
    }
    internal::checkPermissionsAndCallGadget(*gadget,gadgetUse,assistantMessage,context,services,yield);
  } catch(const std::exception& error) {
    const std::string detailed="Error calling tool ("+gadget->name+"): "+error.what();
    Json args={{"content",Json::array({internal::createResult(gadgetId,
      "<tool_use_error>"+detailed+"</tool_use_error>")})},
      {"toolUseResult",detailed},{"sourceToolAssistantUUID",internal::field(assistantMessage,"uuid")}};
    yield(Update{services.createUserMessage(args),{},gadgetId});
  }
}

} // namespace batbox::execution
