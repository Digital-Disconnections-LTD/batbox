#pragma once
#include "gadget_results/Storage.hpp"
#include "gadgets/Gadget.hpp"
#include "user_input/BashInput.hpp"

namespace batbox::user_input {
struct IndigenousBashInputConfig {
  const gadgets::Gadget& bash;
  const gadgets::Gadget* powerShell=nullptr;
  gadgets::GadgetCallContext callContext;
  gadget_results::StorageImports storage;
  Json localResultOverrides=Json::object();
  std::function<bool()> isPowerShellGadgetEnabled;
  std::function<std::string()> resolveDefaultShell;
  std::function<void(const std::string&,const std::optional<Json>&)> showProgress;
  std::function<void()> clearProgress;
};
BashInputServices bindIndigenousBashInput(event_loop::Loop& loop,
  messages::Services& messages,IndigenousBashInputConfig config);
} // namespace batbox::user_input
