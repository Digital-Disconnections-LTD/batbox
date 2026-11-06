#pragma once

#include "runtime/JsonValue.hpp"
#include <functional>

namespace batbox::model {

struct PostStreamInputs {
  const Json* responseHeaders = nullptr;
};
struct PostStreamServices {
  std::function<void(const Json&)> extractQuotaStatusFromHeaders;
};

void processCompletedStream(const PostStreamInputs& input,
  const PostStreamServices& services);

} // namespace batbox::model
