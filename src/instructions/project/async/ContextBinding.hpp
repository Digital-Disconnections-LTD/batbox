#pragma once
#include "context/AsyncContext.hpp"
#include "instructions/project/async/AsyncDiscovery.hpp"

namespace batbox::instructions::project::async {
// Stash and imported services must outlive the installed context callback.
void bindMemoryContextServices(context::AsyncServices& context,
  MemoryDiscoveryStash& stash,Services services);
} // namespace batbox::instructions::project::async
