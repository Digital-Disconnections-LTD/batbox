#pragma once
#include "runtime/JsonValue.hpp"
#include <optional>
#include <stdexcept>
#include <string>

namespace batbox::transport {
struct APIError : std::runtime_error {
  std::optional<long> status;
  Json body;
  Json headers;
  std::string requestId;
  APIError(std::optional<long> status, Json body, Json headers, std::string message = {});
  const char* name() const { return "Error"; }
  virtual const char* kind() const { return "APIError"; }
};
struct APIConnectionError : APIError {
  std::string code;
  explicit APIConnectionError(std::string message = "Connection error.", std::string code = {});
  const char* kind() const override { return "APIConnectionError"; }
};
struct APIConnectionTimeoutError : APIConnectionError {
  explicit APIConnectionTimeoutError(std::string message = "Request timed out.");
  const char* kind() const override { return "APIConnectionTimeoutError"; }
};
struct APIUserAbortError : APIError {
  APIUserAbortError();
  const char* kind() const override { return "APIUserAbortError"; }
};
// Model requests never follow redirects: libcurl would carry custom headers
// such as x-api-key to the new address. This is the error for a 3xx answer;
// it names the status and the Location, and the base URL to set instead when
// the Location ends with the path Batbox appended to the base URL.
APIError redirectError(long status, const Json& headers, const std::string& path);
} // namespace batbox::transport
