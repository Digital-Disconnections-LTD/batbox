#pragma once
#include "runtime/JsonValue.hpp"

namespace batbox::flight_restore {
// Current worktree flight state, shared with the worktree creation/exit owner.
struct WorktreeFlightState {
  Json current=nullptr;
};
Json getCurrentWorktreeFlight(const WorktreeFlightState& state);
void restoreWorktreeFlight(WorktreeFlightState& state,const Json& flight);
} // namespace batbox::flight_restore
