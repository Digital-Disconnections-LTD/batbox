#include "attachments/context/ContextCollectors.hpp"
#include "runtime/OrderedStringSet.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::attachments::context {
namespace {
bool flag(const DeferralConfig& config,const std::string& name){
  if(!config.feature)throw std::invalid_argument("Tool deferral feature import required");
  return config.feature(name);
}
bool jsLess(const std::string& left,const std::string& right){
  return value::utf16CodeUnits(left)<value::utf16CodeUnits(right);
}
}
bool isDeferredGadget(const gadgets::Gadget& gadget,const DeferralConfig& config){
  if(gadget.alwaysLoad==true)return false;
  if(gadget.isMcp==true)return true;
  if(gadget.name==config.searchGadgetName)return false;
  if(flag(config,"FORK_SUBAGENT")&&gadget.name==config.batGadgetName){
    if(!config.forkSubagentEnabled)
      throw std::invalid_argument("Forked-agent feature import required");
    if(config.forkSubagentEnabled())return false;
  }
  if((flag(config,"KAIROS")||flag(config,"KAIROS_BRIEF"))&&
     config.briefGadgetName&&gadget.name==*config.briefGadgetName)return false;
  if(flag(config,"KAIROS")&&config.sendUserFileGadgetName&&
     gadget.name==*config.sendUserFileGadgetName){
    if(!config.replBridgeActive)
      throw std::invalid_argument("Interactive bridge import required");
    if(config.replBridgeActive())return false;
  }
  return gadget.shouldDefer==true;
}
bool isGadgetSearchGadgetAvailable(const gadgets::Gadgets& pool,
    const std::string& name){
  for(const auto* gadget:pool)if(gadget&&gadgets::gadgetMatchesName(*gadget,name))
    return true;
  return false;
}
std::optional<Json> getDeferredGadgetsDelta(const gadgets::Gadgets& pool,
    const Json& messages,const DeferralConfig& config){
  runtime::OrderedStringSet announced;
  if(messages.is_array())for(const auto& message:messages){
    if(value::property(message,"type")!="attachment")continue;
    const auto& attachment=value::property(message,"attachment");
    if(value::property(attachment,"type")!="deferred_tools_delta")continue;
    const auto& added=value::property(attachment,"addedNames");
    if(added.is_array())for(const auto& name:added)
      if(name.is_string())announced.insert(name.get<std::string>());
    const auto& removed=value::property(attachment,"removedNames");
    if(removed.is_array())for(const auto& name:removed)
      if(name.is_string())announced.erase(name.get<std::string>());
  }
  runtime::OrderedStringSet deferredNames,poolNames;
  std::vector<const gadgets::Gadget*> added;
  for(const auto* gadget:pool){
    if(!gadget)continue;
    poolNames.insert(gadget->name);
    if(!isDeferredGadget(*gadget,config))continue;
    deferredNames.insert(gadget->name);
    if(!announced.contains(gadget->name))added.push_back(gadget);
  }
  std::vector<std::string> removed;
  for(const auto& name:announced)
    if(!deferredNames.contains(name)&&!poolNames.contains(name))
      removed.push_back(name);
  if(added.empty()&&removed.empty())return std::nullopt;
  std::vector<std::string> names,lines;
  for(const auto* gadget:added){names.push_back(gadget->name);lines.push_back(gadget->name);}
  std::sort(names.begin(),names.end(),jsLess);
  std::sort(lines.begin(),lines.end(),jsLess);
  std::sort(removed.begin(),removed.end(),jsLess);
  return Json{{"addedNames",names},{"addedLines",lines},
    {"removedNames",removed}};
}
} // namespace batbox::attachments::context
