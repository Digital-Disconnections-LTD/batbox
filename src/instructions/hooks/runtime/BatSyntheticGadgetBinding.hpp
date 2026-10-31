#pragma once
#include "instructions/hooks/runtime/BatOwnerQueryBinding.hpp"
#include "gadgets/synthetic_output/SyntheticOutput.hpp"

namespace batbox::instructions::hooks::runtime {
void bindBatSyntheticOutputGadget(BatHookServices& bat,
  std::string syntheticOutputGadgetName);
execution::Context makeBatHookQueryContext(
  const BatQueryRequest& request,execution::Context parent,
  const std::function<bool(const std::string&)>& isBatDisallowedGadget,
  const std::string& syntheticOutputGadgetName);
} // namespace batbox::instructions::hooks::runtime
