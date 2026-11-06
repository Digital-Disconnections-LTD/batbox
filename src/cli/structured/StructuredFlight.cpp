#include "cli/structured/StructuredFlight.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::cli::structured {
StructuredFlight::StructuredFlight(event_loop::Loop& loop,
    StructuredFlightServices services):services_(std::move(services)){
  if(!services_.nextChunk||!services_.writeOutput)
    throw std::invalid_argument("Structured IO imports required");
  InputReaderServices inputServices;
  inputServices.nextChunk=services_.nextChunk;
  // Parse messages without collecting their types for diagnostics.
  inputServices.logParsedMessageType=[](const std::string&){};
  inputServices.processLine=services_.processLine;
  reader_=std::make_shared<InputReader>(loop,std::move(inputServices));
}
event_loop::Deferred<StreamItem<Json>> StructuredFlight::nextInput(){
  return reader_->next();
}
void StructuredFlight::prependUserMessage(const std::string& content){
  reader_->prependUserMessage(content);
}
event_loop::Deferred<event_loop::Unit> StructuredFlight::write(
    const Json& message){return services_.writeOutput(message);}
}
