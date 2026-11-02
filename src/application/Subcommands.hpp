#pragma once
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
} // namespace batbox::application
