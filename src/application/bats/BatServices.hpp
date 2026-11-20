#pragma once
#include "application/bats/IndigenousQueryBridge.hpp"
#include "bats/BatGadgetCall.hpp"

namespace batbox::application::bats {
// Root application supplies the services that own poetryEntries, hooks,
// transcripts, worktrees and mission state. This binder installs the actual
// RunBat/AsyncQuery path in both foreground and background gadget modes.
struct BatServicesInputs {
  batbox::bats::BatGadgetCallServices gadget;
  batbox::bats::RunBatPreflightServices preflight;
  batbox::bats::RunBatSetupServices setup;
  batbox::bats::RunBatLoopServices loop;
  std::shared_ptr<IndigenousQueryBridge> query;
};
batbox::bats::BatGadgetCallServices makeBatGadgetServices(BatServicesInputs inputs);
} // namespace batbox::application::bats
