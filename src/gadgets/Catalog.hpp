#pragma once
#include "gadgets/Gadget.hpp"
#include <functional>
#include <string>

namespace batbox::gadgets {

struct CatalogSettings {
  std::function<bool(const Gadget&, const Json&)> getDenyRuleForGadget;
  std::function<int(const std::string&, const std::string&)> localeCompare;
};

Gadgets filterGadgetsByDenyRules(const Gadgets& gadgets, const Json& permissionContext, const CatalogSettings& settings);

} // namespace batbox::gadgets
