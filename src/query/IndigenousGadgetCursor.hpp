#pragma once
#include "query/AsyncGadgetTurn.hpp"

namespace batbox::query {
AsyncGadgetUpdateCursor makeIndigenousGadgetUpdatesCursor(event_loop::Loop& loop,
  const std::vector<Json>& blocks,const Json& assistantMessages,
  const std::shared_ptr<PreparedIteration>& prepared,
  execution::Services& executionServices,const std::string& concurrencyValue);

void bindIndigenousGadgetUpdates(event_loop::Loop& loop,AsyncGadgetTurnServices& services);
} // namespace batbox::query
