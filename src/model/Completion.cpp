#include "model/Completion.hpp"
#include <stdexcept>

namespace batbox::model {

void completeQueryModel(const CompletionState& state,
                        const CompletionServices& services) {
  if (!services.stashedMicrocompactFeature)
    throw std::invalid_argument("Cached microcompact feature import is required");
  if (services.stashedMicrocompactFeature() && state.stashedMCEnabled) {
    if (!services.markGadgetsSentToAPIState)
      throw std::invalid_argument("Mark tools sent import is required");
    services.markGadgetsSentToAPIState();
  }
  if (state.streamRequestId && !state.streamRequestId->empty()) {
    if (!services.hasBatContext)
      throw std::invalid_argument("Agent context import is required");
    if (!services.hasBatContext()) {
      if (!state.options)
        throw std::invalid_argument("Query options are required");
      const auto& source = value::property(*state.options,"querySource");
      if (!source.is_string())
        throw std::invalid_argument("Query source must be a string");
      const std::string querySource=source.get<std::string>();
      if (querySource.starts_with("repl_main_thread") || querySource == "sdk") {
        if (!services.setLastMainRequestId)
          throw std::invalid_argument("Last request ID import is required");
        services.setLastMainRequestId(*state.streamRequestId);
      }
    }
  }
}

} // namespace batbox::model
