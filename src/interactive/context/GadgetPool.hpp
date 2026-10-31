#pragma once

#include "gadgets/Gadget.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <string>
#include <unordered_set>

namespace batbox::interactive::context {
struct GadgetPoolServices {
  std::function<int(const std::string&,const std::string&)> localeCompare;
  std::function<bool(const std::string&)> feature;
  std::function<bool()> isCoordinatorMode;
  std::unordered_set<std::string> coordinatorAllowedNames;
};
bool isPrActivitySubscriptionGadget(const std::string& name);
gadgets::Gadgets applyCoordinatorGadgetFilter(const gadgets::Gadgets& available,
  const GadgetPoolServices& services);
gadgets::Gadgets mergeAndFilterGadgets(const gadgets::Gadgets& initial,
  const gadgets::Gadgets& assembled,const std::string& permissionMode,
  const GadgetPoolServices& services);
Json mergeClients(const Json& initial,const Json& connected);
} // namespace batbox::interactive::context
