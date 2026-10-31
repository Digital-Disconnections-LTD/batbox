#include "gadgets/live_roster/ContextBinding.hpp"
#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace batbox::gadgets::live_roster {
void bindContextGadgets(interactive::context::ContextBindings& context,
    std::shared_ptr<Roster> roster,
    std::function<CatalogImports(const Snapshot&)> fullCatalogImports){
  if(!roster)throw std::invalid_argument("Live tool registry required");
  if(!context.getMcpGadgets){
    context.getMcpGadgets=[roster](const app_state::AppStateRef&){
      auto snapshot=roster->current();
      if(!snapshot)throw std::invalid_argument("Tool snapshot required");
      return snapshot->mcp;
    };
  }
  const auto settings=context.catalogSettings;
  context.assembleGadgets=[roster,settings,
      imports=std::move(fullCatalogImports)](
      const Json& permissionContext,const Gadgets& mcpGadgets){
    auto snapshot=roster->current();
    if(!snapshot)throw std::invalid_argument("Tool snapshot required");
    if(imports)return assembleGadgetPool(permissionContext,mcpGadgets,
      imports(*snapshot),settings);
    if(!settings.getDenyRuleForGadget||!settings.localeCompare)
      throw std::invalid_argument("Permission and locale tool bindings required");
    Gadgets base=filterGadgetsByDenyRules(snapshot->base,permissionContext,settings);
    Gadgets enabled;
    for(const auto* gadget:base){
      if(!gadget->isEnabled)throw std::invalid_argument(
        "Tool missing enabled method: "+gadget->name);
      if(gadget->isEnabled())enabled.push_back(gadget);
    }
    Gadgets mcp=filterGadgetsByDenyRules(mcpGadgets,permissionContext,settings);
    auto byName=[&](const Gadget* left,const Gadget* right){
      return settings.localeCompare(left->name,right->name)<0;};
    std::stable_sort(enabled.begin(),enabled.end(),byName);
    std::stable_sort(mcp.begin(),mcp.end(),byName);
    Gadgets result;
    std::unordered_set<std::string> seen;
    for(const auto* gadget:enabled)
      if(seen.insert(gadget->name).second)result.push_back(gadget);
    for(const auto* gadget:mcp)
      if(seen.insert(gadget->name).second)result.push_back(gadget);
    return result;
  };
}
} // namespace batbox::gadgets::live_roster
