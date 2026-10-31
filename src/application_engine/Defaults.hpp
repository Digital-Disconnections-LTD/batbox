#pragma once
#include "application_engine/Engine.hpp"
#include "application_hooks/Provider.hpp"
#include "application_poetry/Provider.hpp"
#include "application_flights/Flight.hpp"
#include "application_permissions/PermissionProvider.hpp"
#include "interactive/submit/CommandQueue.hpp"
#include "memory_prefetch/Prefetch.hpp"
#include "services/tokenEstimation.hpp"
#include "app_state/State.hpp"

namespace batbox::application_engine {
// Host fills the core Engine inputs (including compaction and execution) and
// supplies live owners for the remaining query services.
struct DefaultInputs {
  Inputs engine;
  std::shared_ptr<application_poetry::Provider> poetry;
  std::shared_ptr<application_flights::Flight> flight;
  std::shared_ptr<application_hooks::Provider> hooks;
  std::shared_ptr<application_permissions::PermissionProvider> permission;
  std::shared_ptr<app_state::AppStateStore> store;
  std::function<std::shared_ptr<bats::BatContext>()> activeBatContext;
  std::function<gadgets::Gadgets()> currentGadgets;
  std::function<interactive::submit::CommandQueue&()> commandQueue;
  std::function<void(const std::string&,const std::string&)> notifyCommandLifecycle;
  std::function<event_loop::Deferred<Json>(const Json&)> resizeImage;
  std::function<query::AsyncMessageCursor(const Json&,
    const execution::Context&,const Json&,const std::string&)>
    getAttachmentMessages;
  AttachmentNormalizer normalizeAttachment;
  std::shared_ptr<memory_prefetch::Services> memoryPrefetch;
  std::function<void(const Json&,const query::Params&,
    const execution::Context&)> postSamplingHooks;
  std::function<void(const Json&)> executePoetrySuggestion,executeAutoDream;
  explicit DefaultInputs(event_loop::Loop& owner):engine(owner){}
};
Inputs makeDefaultEngineInputs(DefaultInputs input);
} // namespace batbox::application_engine
