#pragma once
#include "flight_restore/FlightRestore.hpp"
#include "flights/async_project/AsyncProject.hpp"
#include "flights/MetadataOperations.hpp"
#include "flights/concurrent/Roster.hpp"
#include "instructions/project/async/AsyncDiscovery.hpp"
#include "poetryEntries/AsyncSections.hpp"
#include "cwd/State.hpp"
#include "flight_restore/WorktreeFlightState.hpp"
#include <memory>

namespace batbox::flight_restore {
// Bind local flight storage, cwd, worktree and stash modules. Bat definitions,
// cost restoration, attribution and recording are supplied as explicit services.
struct IndigenousComponents {
  std::shared_ptr<flights::async_project::AsyncProject> project;
  std::shared_ptr<flights::MetadataOperations> metadata;
  std::shared_ptr<flights::concurrent::Roster> concurrent;
  std::shared_ptr<instructions::project::async::MemoryDiscoveryStash> memoryStash;
  std::shared_ptr<poetryEntries::AsyncSectionStash> poetrySections;
  std::shared_ptr<cwd::State> cwdState;
  std::shared_ptr<WorktreeFlightState> worktreeState;
  std::function<void()> clearBetaHeaderLatches;
};
void bindIndigenousComponents(Services& services,const IndigenousComponents& components);
} // namespace batbox::flight_restore
