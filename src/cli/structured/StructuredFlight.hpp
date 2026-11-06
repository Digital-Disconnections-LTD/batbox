#pragma once
#include "cli/structured/InputReader.hpp"
#include <memory>

namespace batbox::cli::structured {
struct StructuredFlightServices {
  std::function<event_loop::Deferred<std::optional<std::string>>()> nextChunk;
  std::function<event_loop::Deferred<event_loop::Unit>(const Json&)> writeOutput;
  ProcessLineServices processLine;
};

// Owner-thread composition of the input and output streams.
class StructuredFlight {
public:
  StructuredFlight(event_loop::Loop& loop,StructuredFlightServices services);
  event_loop::Deferred<StreamItem<Json>> nextInput();
  void prependUserMessage(const std::string& content);
  event_loop::Deferred<event_loop::Unit> write(const Json& message);
private:
  StructuredFlightServices services_;
  std::shared_ptr<InputReader> reader_;
};
}
