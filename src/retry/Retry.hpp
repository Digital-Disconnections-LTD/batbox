#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>

namespace batbox::retry {
struct ErrorRecord {
  enum class Kind { Api, Connection, Abort, Other } kind = Kind::Other;
  std::optional<long> status;
  Json body = Json::object();
  Json headers = Json::object();       // Headers.get semantics
  Json legacyHeaders = Json::object(); // direct error.headers[key] semantics
  std::string message;
  std::string code;
  std::string name = "Error";
  std::shared_ptr<ErrorRecord> cause;
  bool isAPIError() const { return kind == Kind::Api || kind == Kind::Connection || kind == Kind::Abort; }
  bool isConnectionError() const { return kind == Kind::Connection; }
};
struct RetryContext {
  std::optional<double> maxTokensOverride;
  std::string model;
  Json thinkingConfig = Json::object();
  std::optional<bool> fastMode;
};
struct RetryOptions {
  std::optional<double> maxRetries;
  std::string model;
  std::optional<std::string> fallbackModel;
  Json thinkingConfig = Json::object();
  std::optional<bool> fastMode;
  std::stop_token signal;
  std::optional<std::string> querySource;
  int initialConsecutive529Errors = 0;
};
struct WireProtocol {
  std::string retryAfter;
  std::string shouldRetry;
  std::string overageDisabledReason;
  std::string rateLimitReset;
};
struct Dependencies {
  Json env = Json::object(); // Batbox configuration values
  WireProtocol wire;
  bool unattendedRetryFeature = false;
  bool bashClassifierFeature = false;
  bool disableKeepAliveOnStaleConnection = false;
  std::function<Json()> getClient;
  std::function<Json(const Json&, double, const RetryContext&)> operation;
  std::function<void(const Json&)> yield;
  std::function<void(double, std::stop_token)> sleep;
  std::function<double()> random;
  std::function<double()> nowMs;
  std::function<Json(const ErrorRecord&, double, double, double)> createSystemAPIErrorMessage;
  std::function<bool()> isFastModeEnabled;
  std::function<bool()> isFastModeCooldown;
  std::function<void(const std::string&)> handleFastModeOverageRejection;
  std::function<void()> handleFastModeRejectedByAPI;
  std::function<void(double, const std::string&)> triggerFastModeCooldown;
  std::function<std::optional<std::string>()> getOAuthAccessToken;
  std::function<void(const std::string&)> handleOAuth401Error;
  std::function<void()> clearApiKeyHelperStash;
  std::function<void()> clearAwsCredentialsStash;
  std::function<void()> clearGcpCredentialsStash;
  std::function<bool()> isSubscriber;
  std::function<bool()> isEnterpriseSubscriber;
  std::function<bool(const std::string&)> isDefaultPremiumModel;
  std::function<void()> disableKeepAlive;
  std::function<void(const std::string&)> logForDebugging;
  std::function<void(const std::string&)> logError;
};
class RetryFailure : public std::exception {
public:
  explicit RetryFailure(ErrorRecord error) : error(std::move(error)) {}
  ErrorRecord error;
  const char* what() const noexcept override { return error.message.c_str(); }
};
class CannotRetryError : public std::runtime_error {
public:
  CannotRetryError(std::optional<ErrorRecord> original, RetryContext context);
  std::optional<ErrorRecord> originalError;
  RetryContext retryContext;
};
class FallbackTriggeredError : public std::runtime_error {
public:
  FallbackTriggeredError(std::string originalModel, std::string fallbackModel);
  std::string originalModel;
  std::string fallbackModel;
};
struct OverflowData { double inputTokens, maxTokens, contextLimit; };
struct ConnectionErrorDetails { std::string code, message; bool isSSLError; };

Json errorToJson(const ErrorRecord& error);
ErrorRecord errorFromJson(const Json& value);
Json contextToJson(const RetryContext& context);
std::optional<ConnectionErrorDetails> extractConnectionErrorDetails(const ErrorRecord& error);
bool shouldRetry529(const std::optional<std::string>& querySource, bool bashClassifierFeature);
bool isPersistentRetryEnabled(const Dependencies& deps);
bool is529Error(const ErrorRecord& error);
std::optional<OverflowData> parseMaxTokensContextOverflowError(const ErrorRecord& error, Dependencies& deps);
std::optional<std::string> getRetryAfter(const ErrorRecord& error, const WireProtocol& wire);
double getRetryDelay(double attempt, std::optional<std::string> retryAfterHeader, double maxDelayMs, Dependencies& deps);
double getDefaultMaxRetries(const Dependencies& deps);
bool shouldRetry(const ErrorRecord& error, Dependencies& deps);
Json withRetry(const RetryOptions& options, Dependencies& deps);
void sleepAbortResponsive(double ms, std::stop_token signal);
} // namespace batbox::retry
