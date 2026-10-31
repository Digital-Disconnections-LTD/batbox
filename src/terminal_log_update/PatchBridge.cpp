#include "terminal_log_update/PatchJson.hpp"

namespace batbox::terminal_log_update {
void writeDiffToTerminal(const std::function<void(const std::string&)>& write,
    const terminal_renderer::Diff& diff,bool skipSyncMarkers,
    const termio::OutputServices& services){
  termio::writeDiffToTerminal(write,patchesToJson(diff),
    skipSyncMarkers,services);
}
} // namespace batbox::terminal_log_update
