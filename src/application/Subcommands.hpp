#pragma once
#include "application/Config.hpp"
#include <optional>
#include <string>
#include <vector>

namespace batbox::application {
// `batbox setup web [...]` runs the bundled gadgets/setup-web.sh with bash.
// `batbox doctor` (or `batbox --doctor`) prints helper and MCP status and
// exits 0 when every check passes, 1 otherwise. Returns nullopt for any
// other command line.
std::optional<int> runSubcommand(const std::vector<std::string>& arguments);
std::string subcommandHelp();
int runDoctor(const std::vector<std::string>& arguments);
// `batbox providers` lists the bundled catalog and local model servers;
// `batbox providers add <id> ...` writes an endpoint without the TUI.
int runProviders(const std::vector<std::string>& arguments);
// No endpoints, profiles or model: the first run needs a provider.
bool needsProviderSetup(const Config& config);
std::string providerSetupHint();
// Starts the interactive first run on a placeholder profile; /providers
// opens on its own and replaces it.
void beginProviderSetup(Config& config);
} // namespace batbox::application
