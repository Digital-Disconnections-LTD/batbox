#include "attachments/context/ContextCollectors.hpp"
#include "utils/tokens.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::attachments::context {
namespace {
event_loop::Deferred<Json> ready(event_loop::Loop& loop,Json value){
  event_loop::Deferred<Json> result(loop);
  result.resolve(std::move(value));
  return result;
}
Json one(const char* type,const std::optional<Json>& value){
  if(!value)return Json::array();
  Json item=*value;
  item["type"]=type;
  return Json::array({std::move(item)});
}
const Json& field(const Json& value,const char* key){return value::property(value,key);}
const Json& history(const Input& input){
  static const Json empty=Json::array();
  return input.messages.is_array()?input.messages:empty;
}
std::string model(const execution::Context& context){
  const auto& value=field(field(context.data,"options"),"mainLoopModel");
  if(!value.is_string())
    throw std::invalid_argument("Main loop model required for attachment collection");
  return value.get<std::string>();
}
bool searchEligible(const gadgets::Gadgets& pool,const std::string& name,
    const execution::Context& context,const Services& services){
  if(!services.gadgetSearchEnabledOptimistic)
    throw std::invalid_argument("Tool-search feature import required");
  if(!services.gadgetSearchEnabledOptimistic())return false;
  if(!services.modelSupportsGadgetReference)
    throw std::invalid_argument("Tool-reference model import required");
  if(!services.modelSupportsGadgetReference(model(context)))return false;
  return isGadgetSearchGadgetAvailable(pool,name);
}
}

void bindContextCollectors(attachments::Services& aggregation,
    Services services){
  bindLeafCollectors(aggregation,services.leaves);
  auto& loop=aggregation.loop;
  aggregation.collectors.deferredGadgetsDelta=[services,&loop](const Input& input,
      execution::Context& context){
    if(!services.deferredGadgetsDeltaEnabled)
      throw std::invalid_argument("Deferred-tools feature import required");
    if(!services.deferredGadgetsDeltaEnabled())return ready(loop,Json::array());
    if(!searchEligible(context.gadgets,services.searchGadgetName,context,
        services))return ready(loop,Json::array());
    return ready(loop,one("deferred_tools_delta",
      getDeferredGadgetsDelta(context.gadgets,history(input),services.deferral)));
  };
  aggregation.collectors.batListingDelta=[services,&loop](const Input& input,
      execution::Context& context){
    // The function owns its feature and gadget-pool guards so live AppState is
    // read only on the branch that consumes permissions.
    const auto& definitions=field(field(context.data,"options"),
      "agentDefinitions");
    return ready(loop,one("agent_listing_delta",
      getBatListingDelta(context.gadgets,definitions,history(input),services)));
  };
  aggregation.collectors.mcpInstructionsDelta=[services,&loop](const Input& input,
      execution::Context& context){
    if(!services.mcpInstructionsDeltaEnabled)
      throw std::invalid_argument("MCP-instruction feature import required");
    if(!services.mcpInstructionsDeltaEnabled())return ready(loop,Json::array());
    std::vector<ClientInstruction> clientSide;
    if(searchEligible(context.gadgets,services.searchGadgetName,context,
        services)){
      if(!services.clientSideInstruction)
        throw std::invalid_argument("Client browser instruction import required");
      if(auto instruction=services.clientSideInstruction())
        clientSide.push_back(std::move(*instruction));
    }
    const auto& clients=field(field(context.data,"options"),"mcpClients");
    return ready(loop,one("mcp_instructions_delta",
      getMcpInstructionsDelta(clients,history(input),clientSide)));
  };
  aggregation.collectors.tokenUsage=[services,&loop](const Input& input,
      execution::Context& context){
    if(!services.tokenUsageAttachmentEnabled)
      throw std::invalid_argument("Token-usage feature import required");
    if(!services.tokenUsageAttachmentEnabled())return ready(loop,Json::array());
    if(!services.effectiveContextWindow)
      throw std::invalid_argument("Effective context-window import required");
    const double window=services.effectiveContextWindow(model(context));
    const double used=tokenCountFromLastAPIResponse(history(input));
    return ready(loop,Json::array({Json{{"type","token_usage"},
      {"used",used},{"total",window},{"remaining",window-used}}}));
  };
}
} // namespace batbox::attachments::context
