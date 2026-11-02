#pragma once
#include "runtime/JsonValue.hpp"
#include <string>

namespace batbox::providers::cloud {
// Provider failures leave this layer in Anthropic's error shape so retry and
// rendering keep matching on status and on error types such as overloaded_error.
struct MappedError {
  long status = 500;
  std::string type = "api_error";
  std::string message;
};
std::string errorTypeForStatus(long status);
Json errorBody(const std::string& type, const std::string& message);
[[noreturn]] void throwMapped(const MappedError& error, Json headers);
// AWS exception names such as ThrottlingException, compared case-insensitively.
MappedError mapAwsException(const std::string& exceptionType, long httpStatus, const std::string& message);
// Google RPC errors ({"error":{"code","message","status"}}) or bodies that are already Anthropic-shaped.
[[noreturn]] void throwGoogleError(long status, const std::string& rawBody, Json headers);
} // namespace batbox::providers::cloud
