#pragma once
#include "bootstrap/ChannelState.hpp"
#include "bootstrap/CoreFlags.hpp"
#include "bootstrap/Costs.hpp"
#include "bootstrap/HookRoster.hpp"
#include "bootstrap/Identity.hpp"
#include "bootstrap/Interaction.hpp"
#include "bootstrap/InvokedSkills.hpp"
#include "bootstrap/ModelState.hpp"
#include "bootstrap/PlanFlightState.hpp"
#include "bootstrap/PoetryState.hpp"
#include "bootstrap/RequestState.hpp"
#include "bootstrap/FlightCron.hpp"
#include "bootstrap/FlightTransitions.hpp"
#include "bootstrap/SlowOperations.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::bootstrap {
struct BootstrapState {
  IdentityState identity;
  CostState costs;
  std::shared_ptr<InteractionState> interaction;
  ModelState model;
  CoreFlags flags;
  RequestState request;
  FlightCronState cron;
  FlightTransitions transitions;
  HookRosterState hooks;
  PlanFlightState plan;
  InvokedSkillsState invokedSkills;
  SlowOperationsState slow;
  PoetryState poetry;
  ChannelState channels;
  double batColorIndex=0;
  std::optional<bool> replBridgeActive;
};
struct BootstrapInitialServices {
  cwd::InitialServices directories;
  std::function<double()> nowMs;
  std::function<std::string()> randomUUID;
  bool developerUser=false;
};
BootstrapState initialBootstrapState(const BootstrapInitialServices& services);
void resetStateForTests(BootstrapState& state,bool testMode,
  const BootstrapInitialServices& services);
BootstrapInitialServices makeIndigenousBootstrapInitialServices(
  bool developerUser=false);
}
