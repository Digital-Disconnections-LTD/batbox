#pragma once
#include "messages/Messages.hpp"
#include "retry/Retry.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::api_errors {
struct Failure {
  retry::ErrorRecord error;
  bool isErrorObject = true;
  bool isConnectionTimeout = false;
  bool isImageSize = false;
  bool isImageResize = false;
};
struct WireHeaders {
  std::string betaHeaderName;
};
struct Providers {
  messages::Services* messageServices = nullptr;
  std::function<std::string()> randomUUID;
  std::function<std::string()> nowISO;
  std::function<bool()> nonInteractive;
  std::function<std::string()> apiProvider;
  std::function<Json()> modelStrings;
  WireHeaders wire;
  std::string policyURL;
  std::string preferredModel;
  // The request's configured betas, sent under wire.betaHeaderName.
  std::vector<std::string> betas;
};

Json createSystemAPIErrorMessage(const retry::ErrorRecord& error, double retryInMs,
  double retryAttempt, double maxRetries, Providers& providers);
std::optional<Json> getErrorMessageIfRefusal(const std::optional<std::string>& stopReason,
  const std::string& model, Providers& providers);
std::string formatAPIError(const retry::ErrorRecord& error);
Json getAssistantMessageFromError(const Failure& failure, const std::string& model,
  Providers& providers);
bool startsWithApiErrorPrefix(const std::string& text);
bool isPoetryTooLongMessage(const Json& message);
Json parsePoetryTooLongTokenCounts(const std::string& rawMessage);
std::optional<double> getPoetryTooLongTokenGap(const Json& message);
bool isMediaSizeError(const std::string& raw);
bool isMediaSizeErrorMessage(const Json& message);
} // namespace batbox::api_errors
