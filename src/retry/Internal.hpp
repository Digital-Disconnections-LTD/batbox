#pragma once
#include "retry/Retry.hpp"
#include "utils/envUtils.hpp"
#include <cmath>
#include <cctype>
#include <optional>
#include <stdexcept>
#include <string>

namespace batbox::retry::internal {
inline Json envValue(const Dependencies& deps, const std::string& key) {
  return deps.env.is_object() && deps.env.contains(key) ? deps.env.at(key) : Json();
}
inline bool envTruthy(const Dependencies& deps, const std::string& key) { return batbox::isEnvTruthy(envValue(deps, key)); }
inline std::string envString(const Dependencies& deps, const std::string& key) {
  Json value = envValue(deps,key); return value.is_string() ? value.get<std::string>() : std::string();
}
inline bool has(const Json& value, const std::string& key) { return value.is_object() && value.contains(key); }
inline Json field(const Json& value, const std::string& key) { return has(value,key) ? value.at(key) : Json(); }
inline std::string stringField(const Json& value, const std::string& key) {
  Json v=field(value,key); return v.is_string() ? v.get<std::string>() : std::string();
}
inline bool contains(const std::string& value, const std::string& needle) { return value.find(needle) != std::string::npos; }
template<class Fn> inline void require(const Fn& callback, const char* name) {
  if (!callback) throw std::logic_error(std::string(name)+" dependency not supplied");
}
inline RetryFailure abortError() {
  ErrorRecord e; e.kind=ErrorRecord::Kind::Abort; e.name="Error"; e.message="Request was aborted.";
  return RetryFailure(std::move(e));
}
inline void checkAbort(std::stop_token token) {
  if (token.stop_requested()) throw abortError();
}
inline std::optional<std::string> headerGet(const Json& headers, const std::string& name) {
  if (!headers.is_object()) return std::nullopt;
  auto lower=[](std::string x) { for (char& c:x) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return x; };
  const std::string key=lower(name);
  for (auto it=headers.begin();it!=headers.end();++it) if (lower(it.key())==key && it.value().is_string()) return it.value().get<std::string>();
  return std::nullopt;
}
bool isTransientCapacityError(const ErrorRecord& error);
bool isStaleConnectionError(const ErrorRecord& error);
bool isOAuthTokenRevokedError(const ErrorRecord& error);
bool isAwsCredentialsProviderError(const ErrorRecord& error);
bool isGoogleAuthLibraryCredentialError(const ErrorRecord& error);
bool isBedrockAuthError(const ErrorRecord& error, const Dependencies& deps);
bool isVertexAuthError(const ErrorRecord& error, const Dependencies& deps);
bool handleAwsCredentialError(const ErrorRecord& error, Dependencies& deps);
bool handleGcpCredentialError(const ErrorRecord& error, Dependencies& deps);
bool isFastModeNotEnabledError(const ErrorRecord& error);
std::optional<double> getRetryAfterMs(const ErrorRecord& error, const Dependencies& deps);
std::optional<double> getRateLimitResetDelayMs(const ErrorRecord& error, Dependencies& deps);
double getMaxRetries(const RetryOptions& options, const Dependencies& deps);
} // namespace batbox::retry::internal
