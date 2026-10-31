#pragma once
#include "event_loop/Deferred.hpp"
#include "model/Preflight.hpp"
#include <optional>

namespace batbox::query {
struct OwnerPreflightAwaits {
  std::function<event_loop::Deferred<bool>()> offSwitchActivated;
  std::function<event_loop::Deferred<std::optional<std::string>>(
    const std::string&)> inferenceProfileBackingModel;
  std::function<event_loop::Deferred<bool>(const std::string&,
    const Json&,const Json&)> gadgetSearchEnabled;
  std::function<event_loop::Deferred<event_loop::Unit>()> loadStashedModule;
  std::function<event_loop::Deferred<event_loop::Unit>()> loadBetasModule;
  std::function<event_loop::Deferred<Json>(const Json&,bool,
    const Json&,const Json&)> gadgetToAPISchema;
  // Test seam for the remaining synchronous preflight steps. Production leaves
  // this unset and uses model::prepareQueryModelFromSeed.
  std::function<model::PreflightState(const model::PreflightInputs&,
    const model::PreflightServices&,model::PreflightState)> remainingSegment;
  std::function<model::PreflightState(const model::PreflightInputs&,
    const model::PreflightServices&,model::PreflightState,bool)> afterGadgetSearchSegment;
  std::function<model::PreflightState(const model::PreflightInputs&,
    const model::PreflightServices&,model::PreflightState)> afterStashedImportsSegment;
  std::function<model::PreflightState(const model::PreflightInputs&,
    const model::PreflightServices&,model::PreflightState,Json)> afterSchemasSegment;
};

event_loop::Deferred<model::PreflightState> startOwnerPreflight(
  event_loop::Loop& loop,model::PreflightInputs input,
  model::PreflightServices services,OwnerPreflightAwaits awaits,
  std::stop_token stop={});
} // namespace batbox::query
