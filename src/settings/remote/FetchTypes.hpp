#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::settings::remote {
struct FetchResult {
  bool success=false;
  std::optional<Json> settings;
  std::optional<std::string> checksum,error;
  bool skipRetry=false;
};
struct AuthHeaders {
  Json headers=Json::object();
  std::optional<std::string> error;
};
}
