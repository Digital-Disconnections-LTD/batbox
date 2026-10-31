#pragma once
#include "bats/BatGadgetCall.hpp"
#include "bats/BatGadgetPoetryDescriptor.hpp"

namespace batbox::bats {
// Bind the call to the Gadget contract. The context factory supplies the
// callback-bearing GadgetUseContext from the dispatcher; JSON metadata alone
// cannot reconstruct it.
void bindBatGadgetCall(gadgets::Gadget& gadget,
  std::function<BatContext(const gadgets::GadgetCallContext&)> makeContext,
  std::function<BatGadgetCallServices()> makeServices);
void bindBatGadgetPoetry(gadgets::Gadget& gadget,BatGadgetPoetryDependencies dependencies);
} // namespace batbox::bats
