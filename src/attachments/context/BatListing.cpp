#include "attachments/context/ContextCollectors.hpp"
#include "attachments/context/Sort.hpp"
#include "runtime/OrderedStringSet.hpp"
#include "settings/PermissionRules.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::attachments::context {
namespace {
const Json& field(const Json& value,const char* name){return value::property(value,name);}
std::string string(const Json& value){
  return value.is_string()?value.get<std::string>():std::string();
}
std::vector<std::string> names(const Json& values){
  std::vector<std::string> result;
  if(values.is_array())for(const auto& value:values)
    if(value.is_string())result.push_back(value.get<std::string>());
  return result;
}
std::string join(const std::vector<std::string>& values){
  std::string result;
  for(const auto& value:values){if(!result.empty())result+=", ";result+=value;}
  return result;
}
}
std::string getGadgetsDescription(const Json& bat){
  const auto allowed=names(field(bat,"tools"));
  const auto denied=names(field(bat,"disallowedTools"));
  if(!allowed.empty()&&!denied.empty()){
    std::vector<std::string> effective;
    for(const auto& gadget:allowed)
      if(std::find(denied.begin(),denied.end(),gadget)==denied.end())
        effective.push_back(gadget);
    return effective.empty()?"None":join(effective);
  }
  if(!allowed.empty())return join(allowed);
  if(!denied.empty())return "All tools except "+join(denied);
  return "All tools";
}
std::string formatBatLine(const Json& bat){
  return "- "+string(field(bat,"agentType"))+": "+
    string(field(bat,"whenToUse"))+" (Tools: "+
    getGadgetsDescription(bat)+")";
}
std::optional<Json> getBatListingDelta(const gadgets::Gadgets& pool,
    const Json& definitions,const Json& messages,const Services& services){
  if(!services.batListInMessages)
    throw std::invalid_argument("Agent-list feature import required");
  if(!services.batListInMessages())return std::nullopt;
  if(!gadgets::findGadgetByName(pool,services.batGadgetName))return std::nullopt;
  const auto& active=field(definitions,"activeAgents");
  const auto& allowedTypes=field(definitions,"allowedAgentTypes");
  runtime::OrderedStringSet mcpServers;
  for(const auto* gadget:pool){
    if(!gadget)continue;
    const auto info=settings::mcpInfoFromString(gadget->name);
    if(info)mcpServers.insert(info->first);
  }
  Json serverNames=Json::array();
  for(const auto& name:mcpServers)serverNames.push_back(name);
  if(!services.getAppState)
    throw std::invalid_argument("Live AppState import required");
  const auto app=services.getAppState();
  if(!app||!app->permissionContext)
    throw std::invalid_argument("Live typed permission context required");
  if(!services.filterDeniedBats)
    throw std::invalid_argument("Agent permission filter import required");
  auto filtered=services.filterDeniedBats(
    bats::filterBatsByMcpRequirements(active,serverNames),
    app->permissionContext->snapshot(),services.batGadgetName);
  if(value::truthy(allowedTypes)){
    Json restricted=Json::array();
    for(const auto& bat:filtered){
      const auto& type=field(bat,"agentType");
      if(allowedTypes.is_array()&&
         std::find(allowedTypes.begin(),allowedTypes.end(),type)!=allowedTypes.end())
        restricted.push_back(bat);
    }
    filtered=std::move(restricted);
  }
  runtime::OrderedStringSet announced;
  if(messages.is_array())for(const auto& message:messages){
    if(field(message,"type")!="attachment")continue;
    const auto& attachment=field(message,"attachment");
    if(field(attachment,"type")!="agent_listing_delta")continue;
    for(const auto& name:names(field(attachment,"addedTypes")))
      announced.insert(name);
    for(const auto& name:names(field(attachment,"removedTypes")))
      announced.erase(name);
  }
  runtime::OrderedStringSet current;
  std::vector<Json> added;
  for(const auto& bat:filtered){
    const auto type=string(field(bat,"agentType"));
    current.insert(type);
    if(!announced.contains(type))added.push_back(bat);
  }
  std::vector<std::string> removed;
  for(const auto& name:announced)
    if(!current.contains(name))removed.push_back(name);
  if(added.empty()&&removed.empty())return std::nullopt;
  std::sort(added.begin(),added.end(),[](const Json& left,const Json& right){
    return localeLess(string(field(left,"agentType")),
      string(field(right,"agentType")));
  });
  std::sort(removed.begin(),removed.end(),utf16Less);
  Json addedTypes=Json::array(),addedLines=Json::array();
  for(const auto& bat:added){
    addedTypes.push_back(field(bat,"agentType"));
    addedLines.push_back(formatBatLine(bat));
  }
  if(!services.subscriptionType)
    throw std::invalid_argument("Subscription type import required");
  return Json{{"addedTypes",addedTypes},{"addedLines",addedLines},
    {"removedTypes",removed},{"isInitial",announced.empty()},
    {"showConcurrencyNote",services.subscriptionType()!="pro"}};
}
} // namespace batbox::attachments::context
