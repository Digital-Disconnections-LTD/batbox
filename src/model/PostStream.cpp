#include "model/PostStream.hpp"
#include <stdexcept>

namespace batbox::model {

void processCompletedStream(const PostStreamInputs& input,
    const PostStreamServices& services) {
  if(input.responseHeaders) {
    if(!services.extractQuotaStatusFromHeaders)
      throw std::invalid_argument("Quota-header import is required");
    services.extractQuotaStatusFromHeaders(*input.responseHeaders);
  }
}

} // namespace batbox::model
