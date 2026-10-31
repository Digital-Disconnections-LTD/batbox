#pragma once
#include "execution/GadgetExecution.hpp"
#include "query_engine/Types.hpp"

namespace batbox::file_state_stash {
void bindExecutionStash(execution::Context& target,
  const query_engine::ProcessContext& source);
} // namespace batbox::file_state_stash
