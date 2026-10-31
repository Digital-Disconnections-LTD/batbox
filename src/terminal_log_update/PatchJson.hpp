#pragma once
#include "terminal_renderer/Frame.hpp"
#include "termio/Output.hpp"

namespace batbox::terminal_log_update {
Json patchesToJson(const terminal_renderer::Diff& diff);
void writeDiffToTerminal(const std::function<void(const std::string&)>& write,
  const terminal_renderer::Diff& diff,bool skipSyncMarkers,
  const termio::OutputServices& services);
} // namespace batbox::terminal_log_update
