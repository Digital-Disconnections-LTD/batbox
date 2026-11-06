#pragma once

#include "gadgets/Gadget.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <string>

namespace batbox::interactive::context {
struct GadgetPoolServices {
  std::function<int(const std::string&,const std::string&)> localeCompare;
};
gadgets::Gadgets mergeAndFilterGadgets(const gadgets::Gadgets& initial,
  const gadgets::Gadgets& assembled,const std::string& permissionMode,
  const GadgetPoolServices& services);
Json mergeClients(const Json& initial,const Json& connected);
} // namespace batbox::interactive::context
