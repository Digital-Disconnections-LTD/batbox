#pragma once
#include "interactive/context/ContextFactory.hpp"
#include "gadgets/live_roster/LiveRoster.hpp"

namespace batbox::gadgets::live_roster {
// Bind the live roster to the interaction context without copying Gadget
// values. The application owns Roster through the entire ContextFactory
// lifetime and releases retired snapshots only when no turn is in flight.
void bindContextGadgets(interactive::context::ContextBindings& context,
  std::shared_ptr<Roster> roster,
  std::function<CatalogImports(const Snapshot&)> fullCatalogImports={});
} // namespace batbox::gadgets::live_roster
