#pragma once
#include "application_bats/IndigenousQueryBridge.hpp"
#include "bats/BatGadgetCall.hpp"

namespace batbox::application_bats {
// Root application supplies the services that own poetryEntries, hooks,
// transcripts, worktrees and mission state. This binder installs the actual
// RunBat/AsyncQuery path in both foreground and background gadget modes.
struct BatServicesInputs {
  bats::BatGadgetCallServices gadget;
  bats::RunBatPreflightServices preflight;
  bats::RunBatSetupServices setup;
  bats::RunBatLoopServices loop;
  std::shared_ptr<IndigenousQueryBridge> query;
};
bats::BatGadgetCallServices makeBatGadgetServices(BatServicesInputs inputs);
} // namespace batbox::application_bats
