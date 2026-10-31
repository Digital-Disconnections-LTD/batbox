#pragma once
#include "bats/RunBat.hpp"
#include "mcp/manager/IndigenousRuntime.hpp"

namespace batbox::application_bats {
bats::BatMcpSetup initializeBatMcpServers(
  const Json& definition,const Json& parentClients,
  const std::shared_ptr<mcp::manager::IndigenousRuntime>& runtime,
  event_loop::Loop::Dispatcher owner,
  const std::function<void(const std::string&)>& diagnostic);
}
