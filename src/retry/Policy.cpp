#include "retry/Internal.hpp"
#include <algorithm>
#include <cmath>
#include <regex>
#include <set>

namespace batbox::retry {
namespace {
constexpr double DEFAULT_MAX_RETRIES = 10;
constexpr double BASE_DELAY_MS = 500;
constexpr double PERSISTENT_RESET_CAP_MS = 6 * 60 * 60 * 1000;
const std::set<std::string> FOREGROUND_529_RETRY_SOURCES = {
  "repl_main_thread", "repl_main_thread:outputStyle:custom",
  "repl_main_thread:outputStyle:Explanatory", "repl_main_thread:outputStyle:Learning",
  "sdk", "agent:custom", "agent:default", "agent:builtin", "compact", "hook_agent",
  "hook_prompt", "verification_agent", "side_question", "auto_mode",
};
}
bool shouldRetry529(const std::optional<std::string>& querySource, bool bashClassifierFeature) {
  return !querySource || FOREGROUND_529_RETRY_SOURCES.count(*querySource) != 0 ||
    (bashClassifierFeature && *querySource == "bash_classifier");
}
bool isPersistentRetryEnabled(const Dependencies& deps) {
  return deps.unattendedRetryFeature ? internal::envTruthy(deps,"BATBOX_UNATTENDED_RETRY") : false;
}
bool is529Error(const ErrorRecord& error) {
  if (!error.isAPIError()) return false;
  return error.status == 529 || internal::contains(error.message,"\"type\":\"overloaded_error\"");
}
std::optional<std::string> getRetryAfter(const ErrorRecord& error, const WireProtocol& wire) {
  Json direct = internal::field(error.legacyHeaders,wire.retryAfter);
  if (direct.is_string() && !direct.get<std::string>().empty()) return direct.get<std::string>();
  return internal::headerGet(error.headers,wire.retryAfter);
}
double getRetryDelay(double attempt, std::optional<std::string> retryAfterHeader, double maxDelayMs, Dependencies& deps) {
  if (retryAfterHeader && !retryAfterHeader->empty()) {
    const double seconds=value::parseInt10(*retryAfterHeader);
    if (!std::isnan(seconds)) return seconds * 1000;
  }
  const double baseDelay=std::min(BASE_DELAY_MS*std::pow(2,attempt-1),maxDelayMs);
  internal::require(deps.random,"random");
  const double jitter=deps.random()*0.25*baseDelay;
  return baseDelay+jitter;
}
std::optional<OverflowData> parseMaxTokensContextOverflowError(const ErrorRecord& error, Dependencies&) {
  if (error.status != 400 || error.message.empty()) return std::nullopt;
  if (!internal::contains(error.message,"input length and `max_tokens` exceed context limit")) return std::nullopt;
  static const std::regex regex(R"(input length and `max_tokens` exceed context limit: (\d+) \+ (\d+) > (\d+))");
  std::smatch match;
  if (!std::regex_search(error.message,match,regex) || match.size()!=4) return std::nullopt;
  if (match[1].str().empty() || match[2].str().empty() || match[3].str().empty()) return std::nullopt;
  const double inputTokens=value::parseInt10(match[1].str());
  const double maxTokens=value::parseInt10(match[2].str());
  const double contextLimit=value::parseInt10(match[3].str());
  if (std::isnan(inputTokens) || std::isnan(maxTokens) || std::isnan(contextLimit)) return std::nullopt;
  return OverflowData{inputTokens,maxTokens,contextLimit};
}
double getDefaultMaxRetries(const Dependencies& deps) {
  std::string configured=internal::envString(deps,"BATBOX_MAX_RETRIES");
  if (!configured.empty()) return value::parseInt10(configured);
  return DEFAULT_MAX_RETRIES;
}
namespace internal {
bool isTransientCapacityError(const ErrorRecord& error) {
  return is529Error(error) || (error.isAPIError() && error.status == 429);
}
bool isStaleConnectionError(const ErrorRecord& error) {
  if (!error.isConnectionError()) return false;
  const auto details=extractConnectionErrorDetails(error);
  return details && (details->code=="ECONNRESET" || details->code=="EPIPE");
}
bool isOAuthTokenRevokedError(const ErrorRecord& error) {
  return error.isAPIError() && error.status==403 && contains(error.message,"OAuth token has been revoked");
}
bool isAwsCredentialsProviderError(const ErrorRecord& error) {
  return error.name=="CredentialsProviderError";
}
bool isGoogleAuthLibraryCredentialError(const ErrorRecord& error) {
  const std::string& msg=error.message;
  return contains(msg,"Could not load the default credentials") ||
    contains(msg,"Could not refresh access token") || contains(msg,"invalid_grant");
}
bool isBedrockAuthError(const ErrorRecord& error, const Dependencies& deps) {
  if (envTruthy(deps,"BATBOX_USE_BEDROCK")) {
    if (isAwsCredentialsProviderError(error) || (error.isAPIError() && error.status==403)) return true;
  }
  return false;
}
bool handleAwsCredentialError(const ErrorRecord& error, Dependencies& deps) {
  if (isBedrockAuthError(error,deps)) {
    require(deps.clearAwsCredentialsStash,"clearAwsCredentialsCache"); deps.clearAwsCredentialsStash(); return true;
  }
  return false;
}
bool isVertexAuthError(const ErrorRecord& error, const Dependencies& deps) {
  if (envTruthy(deps,"BATBOX_USE_VERTEX")) {
    if (isGoogleAuthLibraryCredentialError(error)) return true;
    if (error.isAPIError() && error.status==401) return true;
  }
  return false;
}
bool handleGcpCredentialError(const ErrorRecord& error, Dependencies& deps) {
  if (isVertexAuthError(error,deps)) {
    require(deps.clearGcpCredentialsStash,"clearGcpCredentialsCache"); deps.clearGcpCredentialsStash(); return true;
  }
  return false;
}
bool isFastModeNotEnabledError(const ErrorRecord& error) {
  return error.isAPIError() && error.status==400 && contains(error.message,"Fast mode is not enabled");
}
std::optional<double> getRetryAfterMs(const ErrorRecord& error, const Dependencies& deps) {
  const auto retryAfter=getRetryAfter(error,deps.wire);
  if (retryAfter && !retryAfter->empty()) {
    const double seconds=value::parseInt10(*retryAfter);
    if (!std::isnan(seconds)) return seconds*1000;
  }
  return std::nullopt;
}
std::optional<double> getRateLimitResetDelayMs(const ErrorRecord& error, Dependencies& deps) {
  const auto resetHeader=headerGet(error.headers,deps.wire.rateLimitReset);
  if (!resetHeader || resetHeader->empty()) return std::nullopt;
  double resetUnixSec;
  try { std::size_t end=0; resetUnixSec=std::stod(value::trim(*resetHeader),&end);
    if (end!=value::trim(*resetHeader).size()) return std::nullopt;
  } catch (...) { return std::nullopt; }
  if (!std::isfinite(resetUnixSec)) return std::nullopt;
  require(deps.nowMs,"nowMs");
  const double delayMs=resetUnixSec*1000-deps.nowMs();
  if (delayMs<=0) return std::nullopt;
  return std::min(delayMs,PERSISTENT_RESET_CAP_MS);
}
double getMaxRetries(const RetryOptions& options, const Dependencies& deps) {
  return options.maxRetries.value_or(getDefaultMaxRetries(deps));
}
}
bool shouldRetry(const ErrorRecord& error, Dependencies& deps) {
  // Rate-limit behavior comes from actual responses and retry policy.
  if (isPersistentRetryEnabled(deps) && internal::isTransientCapacityError(error)) return true;
  if (internal::envTruthy(deps,"BATBOX_REMOTE") && (error.status==401 || error.status==403)) return true;
  if (internal::contains(error.message,"\"type\":\"overloaded_error\"")) return true;
  if (parseMaxTokensContextOverflowError(error,deps)) return true;
  const auto retryHeader=internal::headerGet(error.headers,deps.wire.shouldRetry);
  if (retryHeader=="true") {
    internal::require(deps.isSubscriber,"isSubscriber");
    if (!deps.isSubscriber()) return true;
    internal::require(deps.isEnterpriseSubscriber,"isEnterpriseSubscriber");
    if (deps.isEnterpriseSubscriber()) return true;
  }
  if (retryHeader=="false") {
    const bool is5xxError=error.status && *error.status>=500;
    if (!(internal::envString(deps,"BATBOX_USER_TYPE")=="ant" && is5xxError)) return false;
  }
  if (error.isConnectionError()) return true;
  if (!error.status || *error.status==0) return false;
  if (error.status==408) return true;
  if (error.status==409) return true;
  if (error.status==429) {
    internal::require(deps.isSubscriber,"isSubscriber");
    if (!deps.isSubscriber()) return true;
    internal::require(deps.isEnterpriseSubscriber,"isEnterpriseSubscriber");
    return deps.isEnterpriseSubscriber();
  }
  if (error.status==401) {
    internal::require(deps.clearApiKeyHelperStash,"clearApiKeyHelperCache");
    deps.clearApiKeyHelperStash(); return true;
  }
  if (internal::isOAuthTokenRevokedError(error)) return true;
  if (*error.status>=500) return true;
  return false;
}
} // namespace batbox::retry
