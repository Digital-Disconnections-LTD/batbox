#pragma once
#include "application_engine/Engine.hpp"
#include "application_flights/Flight.hpp"
#include "compaction/Summary.hpp"

namespace batbox::application_engine {
// The engine is resolved when a compact actually runs: the compactor is built
// before the root Engine, while its fork must use that engine's live bindings.
struct CompactForkInputs {
  event_loop::Loop& owner;
  std::shared_ptr<messages::Services> messages;
  std::shared_ptr<application_flights::Flight> flight;
  std::function<std::shared_ptr<Engine>()> currentEngine;
  query::NotifyLifecycle notifyCommandLifecycle;
  bool poetryStashSharingEnabled=true;
  explicit CompactForkInputs(event_loop::Loop& loop):owner(loop){}
};

// Bind stash sharing before Compactor::create(). The branch
// requires a typed StashSafeParams snapshot; the existing streaming summary
// remains the fallback if no snapshot is available or the fork fails.
void bindCompactSummaryFork(compaction::SummaryServices& summary,
  CompactForkInputs inputs);
} // namespace batbox::application_engine
