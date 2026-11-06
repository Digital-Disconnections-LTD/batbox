#pragma once
#include "memory_prefetch/Prefetch.hpp"
#include "side_query/SideQuery.hpp"

namespace batbox::memory_prefetch {
void bindSideQuery(Services& memory,side_query::Services side);
} // namespace batbox::memory_prefetch
