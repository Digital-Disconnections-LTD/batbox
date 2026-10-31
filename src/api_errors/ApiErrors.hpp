#pragma once
#include "messages/Messages.hpp"
#include "retry/Retry.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::api_errors {
struct Failure {
  retry::ErrorRecord error;
  bool isErrorObject = true;
  bool isConnectionTimeout = false;
  bool isImageSize = false;
  bool isImageResize = false;
};
struct WireHeaders {
  std::string representativeClaim;
  std::string overageStatus;
  std::string reset;
  std::string overageReset;
  std::string overageDisabledReason;
  std::string betaHeaderName;
};
struct Providers {
  messages::Services* messageServices = nullptr;
  std::function<std::string()> randomUUID;
  std::function<std::string()> nowISO;
  std::function<bool()> nonInteractive;
  std::function<bool()> subscriber;
  std::function<bool(bool)> shouldProcessRateLimits;
  std::function<std::optional<std::string>()> oauthAccessToken;
  std::function<std::string()> apiKeySource;
  std::function<std::string()> apiProvider;
  std::function<Json()> modelStrings;
  std::function<bool(const std::string&)> isDefaultPremiumModel;
  std::function<std::optional<std::string>(const Json&, const std::string&)> rateLimitErrorMessage;
  Json env = Json::object();
  WireHeaders wire;
  std::string policyURL;
  std::string preferredModel;
  std::string statusURL;
  std::string usageURL;
  std::string betaHeaderValue;
};

Json createSystemAPIErrorMessage(const retry::ErrorRecord& error, double retryInMs,
  double retryAttempt, double maxRetries, Providers& providers);
std::optional<Json> getErrorMessageIfRefusal(const std::optional<std::string>& stopReason,
  const std::string& model, Providers& providers);
std::string categorizeRetryableAPIError(const retry::ErrorRecord& error);
std::string formatAPIError(const retry::ErrorRecord& error);
Json getAssistantMessageFromError(const Failure& failure, const std::string& model,
  Providers& providers);
bool startsWithApiErrorPrefix(const std::string& text);
bool isPoetryTooLongMessage(const Json& message);
Json parsePoetryTooLongTokenCounts(const std::string& rawMessage);
std::optional<double> getPoetryTooLongTokenGap(const Json& message);
bool isMediaSizeError(const std::string& raw);
bool isMediaSizeErrorMessage(const Json& message);
bool isValidAPIMessage(const Json& value);
std::optional<std::string> extractUnknownErrorFormat(const Json& value);
} // namespace batbox::api_errors
