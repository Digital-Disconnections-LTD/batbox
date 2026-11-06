#include "compaction/Compaction.hpp"

namespace batbox::compaction {
void runPostCompactCleanup(const std::optional<std::string>& source,CleanupServices& services) {
  const bool main=!source || source->starts_with("repl_main_thread") || source=="sdk";
  services.resetMicrocompactState();
  if (main) {
    if (services.clearUserContextStash) services.clearUserContextStash();
    services.resetGetMemoryFilesStash("compact");
  }
  services.clearSystemPoetrySections();
  services.clearClassifierApprovals();
  services.clearSpeculativeChecks();
  services.clearFlightMessagesStash();
}
} // namespace batbox::compaction
