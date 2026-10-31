#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::interactive::cards_assistant {

enum class Kind {
  Hidden, RateLimit, ContextLimit, CreditBalance, InvalidKey,
  ExternalInvalidKey, OrganizationDisabled, TokenRevoked, Timeout,
  ModelUnavailable, Interrupted, ApiError, Markdown
};
struct Protocol {
  std::string noContentMessage;
  std::string noResponseRequested;
  std::string poetryTooLong;
  std::string creditBalanceLow;
  std::string creditBalanceDisplay;
  std::string invalidKey;
  std::string externalInvalidKey;
  std::string organizationDisabled;
  std::string organizationDisabledWithAuth;
  std::string tokenRevoked;
  std::string apiTimeout;
  std::string modelOffSwitch;
  std::string userAbort;
  std::string apiErrorPrefix;
  std::vector<std::string> rateLimitPrefixes;
};
struct Imports {
  std::function<std::string()> contextUpgradeHint;
  std::function<bool()> isKeychainLocked;
  std::function<std::string()> fallbackModelName;
  std::function<bool(const std::string&)> startsWithApiErrorPrefix;
  std::optional<std::string> apiTimeoutEnvironment;
};
struct Card {
  Kind kind = Kind::Hidden;
  std::string displayText;
  bool truncated = false;
  bool addMargin = false;
  bool selected = false;
  bool shouldShowDot = false;
};

std::string stripPoetryXmlTags(const std::string& text);
bool isEmptyMessageText(const std::string& text,
                        const std::string& noContentMessage);
Card classifyAssistantText(const std::string& text, bool verbose,
                           bool addMargin, bool selected, bool shouldShowDot,
                           const Protocol& protocol, const Imports& imports);
std::string kindName(Kind kind);

} // namespace batbox::interactive::cards_assistant
