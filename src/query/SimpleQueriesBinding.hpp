#pragma once
#include "model/SimpleQueries.hpp"
#include "query/OwnerModelCursor.hpp"

namespace batbox::query {
event_loop::Deferred<model::MessageRef> collectLastAssistantMessage(
  event_loop::Loop& loop,AsyncModelCursor cursor,std::stop_token signal={});
}
