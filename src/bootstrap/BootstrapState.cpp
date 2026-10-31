#include "bootstrap/BootstrapState.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::bootstrap {
BootstrapState initialBootstrapState(const BootstrapInitialServices& services){
  if(!services.nowMs||!services.randomUUID)
    throw std::invalid_argument("Bootstrap clock and UUID bindings required");
  BootstrapState state;
  state.identity.directories=cwd::initialState(services.directories);
  state.costs=initialCostState(services.nowMs);
  state.interaction=makeInteractionState(InteractionServices{services.nowMs,{}});
  state.identity.flightId=services.randomUUID();
  state.identity.randomUUID=services.randomUUID;
  if(services.developerUser)state.replBridgeActive=false;
  return state;
}
void resetStateForTests(BootstrapState& state,bool testMode,
  const BootstrapInitialServices& services){
  if(!testMode)
    throw std::runtime_error("resetStateForTests can only be called in tests");
  auto next=initialBootstrapState(services);
  auto flightSwitched=state.identity.flightSwitched;
  state=std::move(next);
  state.identity.flightSwitched=std::move(flightSwitched);
  state.identity.flightSwitched.clear();
}
}
