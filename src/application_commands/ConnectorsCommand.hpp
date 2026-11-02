#pragma once
#include "application/Config.hpp"
#include "application_connectors/Status.hpp"
#include "event_loop/Loop.hpp"
#include "interactive/CommandUi.hpp"
#include "mcp/manager/IndigenousRuntime.hpp"
#include "user_input/UiCommandRouter.hpp"
#include <functional>
#include <memory>
#include <vector>

namespace batbox::application_commands {
using HelperChecks=std::vector<application_connectors::HelperCheck>;
struct ConnectorsInputs {
  application::Config config;
  std::shared_ptr<mcp::manager::IndigenousRuntime> mcpRuntime;
  bool interactive=true;
  // Runs on a worker thread; probes Scrapling and SearXNG by default.
  std::function<HelperChecks()> checkWeb;
  // Live MCP states; the running MCP manager by default.
  std::function<HelperChecks()> checkMcp;
};
// The dialog for one set of results. Exposed for golden frames.
interactive::UiRequest connectorsRequest(const HelperChecks& web,const HelperChecks& mcp);
// Plain text for --print and for the "Set up now" action.
std::string connectorsText(const HelperChecks& web,const HelperChecks& mcp);
std::string setupWebInstructions();
// /connectors shows Scrapling, SearXNG and MCP servers with status and fixes.
user_input::CommandEntry makeConnectorsCommand(event_loop::Loop& owner,
  std::shared_ptr<user_input::UiCommandRouter> router,ConnectorsInputs inputs);
} // namespace batbox::application_commands
