#pragma once
#include "application/engine/Engine.hpp"
#include "application/hooks/Provider.hpp"
#include "application/poetry/Provider.hpp"
#include "application/flights/Flight.hpp"
#include "application/permissions/PermissionProvider.hpp"
#include "interactive/submit/CommandQueue.hpp"
#include "memory_prefetch/Prefetch.hpp"
#include "services/tokenEstimation.hpp"
#include "app_state/State.hpp"
#include <optional>

namespace batbox::application::engine {
// Host fills the core Engine inputs (including compaction and execution) and
// supplies live owners for the remaining query services.
struct DefaultInputs {
  Inputs engine;
  std::shared_ptr<application::poetry::Provider> poetry;
  std::shared_ptr<application::flights::Flight> flight;
  std::shared_ptr<application::hooks::Provider> hooks;
  std::shared_ptr<application::permissions::PermissionProvider> permission;
  std::shared_ptr<app_state::AppStateStore> store;
  std::function<std::shared_ptr<batbox::bats::BatContext>()> activeBatContext;
  std::function<batbox::gadgets::Gadgets()> currentGadgets;
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
// Post-compaction user-context merge. A main-thread query adopts the live
// provider keys wholesale. A Bat context assembled without "batboxMd" (its
// definition omitted it) keeps that omission: a provider refresh must not
// re-inflate the trim that keeps Bat requests short.
Json mergeRefreshedUserContext(const execution::Context& context,
    const Json& previous,const Json& fresh);
// Turn-finish gadget pool refresh. A main-thread context adopts the live
// catalog. A bat context (data["agentId"] set) owns its resolved pool and
// adopts only MCP gadgets that came live after setup; nullopt means nothing
// changed. Wholesale catalog adoption re-renders every bat continuation
// turn's tool list and overflows small-context models.
std::optional<batbox::gadgets::Gadgets> refreshGadgetsForContext(
    const execution::Context& context,const batbox::gadgets::Gadgets& live);
Inputs makeDefaultEngineInputs(DefaultInputs input);
} // namespace batbox::application::engine
