#include "gadgets/Catalog.hpp"
#include <stdexcept>

namespace batbox::gadgets {

Gadgets filterGadgetsByDenyRules(const Gadgets& gadgets,const Json& permissionContext,const CatalogSettings& settings) {
  if(!settings.getDenyRuleForGadget)throw std::invalid_argument("Unbound getDenyRuleForTool");
  Gadgets filtered;
  for(const Gadget* gadget:gadgets)
    if(!settings.getDenyRuleForGadget(*gadget,permissionContext))filtered.push_back(gadget);
  return filtered;
}

} // namespace batbox::gadgets
