#pragma once
#include "event_loop/Deferred.hpp"
#include "gadgets/bash/AsyncRunShellCommand.hpp"
#include "gadgets/bash/AsyncBashPersistence.hpp"
#include "gadgets/bash/BashGadget.hpp"

namespace batbox::bash_gadget {
// Mission roster and policy services for the BashGadget.call generator path.
// No callback has a production fallback.
struct AsyncMissionHost {
  std::function<event_loop::Deferred<gadgets::GadgetResult>(const Json&,
    const gadgets::GadgetCallContext&,const Json&)> applySedEdit;
  std::function<event_loop::Deferred<std::string>(const AsyncRunInput&,
    const AsyncShellCommand&,const gadgets::GadgetCallContext&)> spawnShellMission;
  std::function<bool(const std::string&,const AsyncRunInput&,
    const AsyncShellCommand&,const gadgets::GadgetCallContext&)> backgroundExistingForegroundMission;
  std::function<std::string(const AsyncRunInput&,const AsyncShellCommand&,
    const gadgets::GadgetCallContext&)> registerForeground;
  std::function<void(const std::string&,const gadgets::GadgetCallContext&)> unregisterForeground;
  std::function<void(const std::string&,const gadgets::GadgetCallContext&)> markMissionNotified;
  std::function<bool(const std::string&)> isAutoBackgroundAllowed;
  // Read live application state when resetting after a command.
  std::function<Json()> getGadgetPermissionContext;
  std::function<double()> getDefaultTimeoutMs,getProgressThresholdMs;
  std::function<void(std::exception_ptr)> onUnobservedError;
  AsyncBashFileOps fileOps;
  std::function<event_loop::Deferred<std::optional<std::string>>(
    const std::string&,const std::optional<std::string>&,
    const std::optional<std::uintmax_t>&)> resizeShellImageOutput;
};

event_loop::Deferred<gadgets::GadgetResult> callAsyncBash(event_loop::Loop& loop,
  std::shared_ptr<const Services> services,std::shared_ptr<const AsyncMissionHost> host,
  Json input,gadgets::GadgetCallContext context,Json parentMessage,
  std::function<void(const gadgets::GadgetProgress&)> onProgress);

// Selectable Gadget factory. Gadget.call posts to the caller-owned long-lived Loop
// and returns its future immediately; the owner must keep Loop::run active.
gadgets::Gadget makeAsyncBashGadget(Services services,event_loop::Loop& loop,
  AsyncMissionHost host);
} // namespace batbox::bash_gadget
