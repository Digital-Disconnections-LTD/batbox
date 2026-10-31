#pragma once

#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>

namespace batbox::model {

struct PostStreamInputs {
  const Json& options;
  const Json& usage;
  const Json& messages;
  const std::optional<std::string>& streamRequestId;
  const Json* responseHeaders = nullptr;
};
struct PostStreamServices {
  std::function<bool()> poetryStashBreakFeature;
  // Starts a local stash-break check without waiting for it to finish.
  std::function<void(const std::string&,const Json&,const Json&,
    const Json&,const Json&,const std::optional<std::string>&)>
    checkResponseForStashBreak;
  std::function<void(const Json&)> extractQuotaStatusFromHeaders;
};

void processCompletedStream(const PostStreamInputs& input,
  const PostStreamServices& services);

} // namespace batbox::model
