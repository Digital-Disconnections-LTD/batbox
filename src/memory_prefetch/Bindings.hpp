#pragma once
#include "memory_prefetch/Prefetch.hpp"
#include "query/AsyncGadgetTurn.hpp"
#include "side_query/SideQuery.hpp"

namespace batbox::memory_prefetch {
void bindSideQuery(Services& memory,side_query::Services side);
void bindQueryPrefetch(query::AsyncEngineServices& engine,
  query::AsyncGadgetTurnServices& gadgetTurn,Services memory);
} // namespace batbox::memory_prefetch
