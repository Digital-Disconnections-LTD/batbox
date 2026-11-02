#pragma once
#include "transport/WireClient.hpp"
#include <string_view>

namespace batbox::providers::cloud {
// Bedrock and Vertex endpoints derive their host from region and project, so base_url is optional.
inline bool isCloudKind(std::string_view kind) {
  return kind == transport::kProviderBedrock || kind == transport::kProviderVertex;
}
} // namespace batbox::providers::cloud
