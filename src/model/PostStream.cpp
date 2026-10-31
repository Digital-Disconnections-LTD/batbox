#include "model/PostStream.hpp"
#include <stdexcept>

namespace batbox::model {

void processCompletedStream(const PostStreamInputs& input,
    const PostStreamServices& services) {
  if(!services.poetryStashBreakFeature)
    throw std::invalid_argument("Prompt cache-break feature import is required");
  if(services.poetryStashBreakFeature()) {
    if(!services.checkResponseForStashBreak)
      throw std::invalid_argument("Cache-break check import is required");
    const auto& source=value::property(input.options,"querySource");
    if(!source.is_string())throw std::invalid_argument("Query source must be a string");
    services.checkResponseForStashBreak(source.get<std::string>(),
      value::property(input.usage,"cache_read_input_tokens"),
      value::property(input.usage,"cache_creation_input_tokens"),
      input.messages,value::property(input.options,"agentId"),
      input.streamRequestId);
  }
  if(input.responseHeaders) {
    if(!services.extractQuotaStatusFromHeaders)
      throw std::invalid_argument("Quota-header import is required");
    services.extractQuotaStatusFromHeaders(*input.responseHeaders);
  }
}

} // namespace batbox::model
