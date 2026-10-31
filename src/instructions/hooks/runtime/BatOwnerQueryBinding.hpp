#pragma once
#include "instructions/hooks/runtime/BatHookExecutor.hpp"
#include "query/AsyncEngine.hpp"

namespace batbox::instructions::hooks::runtime {
struct BatOwnerQueryBindings {
  // Bind GadgetUseContext into the Indigenous query Context with actual Gadget
  // objects, app state and permission services.
  std::function<execution::Context(const BatQueryRequest&)> buildContext;
  std::function<std::shared_ptr<bats::BatContext>(const BatQueryRequest&)>
    indigenousContext;
  std::function<query::AsyncEngineServices(const BatQueryRequest&)> buildServices;
};
void bindBatOwnerQuery(event_loop::Loop& loop,BatHookServices& bat,
  BatOwnerQueryBindings bindings);
} // namespace batbox::instructions::hooks::runtime
