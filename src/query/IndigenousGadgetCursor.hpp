#pragma once
#include "query/AsyncGadgetTurn.hpp"

namespace batbox::query {
AsyncGadgetUpdateCursor makeIndigenousGadgetUpdatesCursor(event_loop::Loop& loop,
  const std::vector<Json>& blocks,const Json& assistantMessages,
  const std::shared_ptr<PreparedIteration>& prepared,
  execution::Services& executionServices,const std::string& concurrencyValue);

AsyncModelCursor makeIndigenousRemainingMessagesCursor(event_loop::Loop& loop,
  const std::shared_ptr<PreparedIteration>& prepared);

void bindIndigenousGadgetUpdates(event_loop::Loop& loop,AsyncGadgetTurnServices& services);
void bindIndigenousAbortRemaining(event_loop::Loop& loop,AsyncEngineServices& services);
} // namespace batbox::query
