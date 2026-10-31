#pragma once
#include "instructions/hooks/runtime/BatHookExecutor.hpp"
#include "instructions/hooks/runtime/FlightHookStore.hpp"

namespace batbox::instructions::hooks::runtime {
void bindBatHookFlightStore(BatHookServices& bat,
  std::shared_ptr<FlightHookStore> store,
  std::string syntheticOutputGadgetName);
} // namespace batbox::instructions::hooks::runtime
