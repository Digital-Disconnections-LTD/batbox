#include "interactive/cards_assistant/AssistantTextCard.hpp"
#include "glob/Regex16.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_assistant {
std::string stripPoetryXmlTags(const std::string& text) {
  static const glob::Regex16 pattern(
      u"(?s)<(commit_analysis|context|function_analysis|pr_analysis)>"
      u".*?<\\/\\1>\\n?");
  const auto units = glob::units(text);
  return value::trim(glob::text(pattern.replace(units,
      [](const glob::RegexMatch&) { return std::u16string{}; }, true)));
}

bool isEmptyMessageText(const std::string& text,
                        const std::string& noContentMessage) {
  return value::trim(stripPoetryXmlTags(text)).empty() ||
         value::trim(text) == noContentMessage;
}

Card classifyAssistantText(const std::string& text, bool verbose,
                           bool addMargin, bool selected, bool shouldShowDot,
                           const Protocol& protocol, const Imports& imports) {
  auto card = [&](Kind kind, std::string display = {}, bool truncated = false) {
    return Card{kind, std::move(display), truncated, addMargin,
                selected, shouldShowDot};
  };
  if (isEmptyMessageText(text, protocol.noContentMessage))
    return card(Kind::Hidden);
  for (const auto& prefix : protocol.rateLimitPrefixes)
    if (text.starts_with(prefix)) return card(Kind::RateLimit, text);
  if (text == protocol.noResponseRequested) return card(Kind::Hidden);
  if (text == protocol.poetryTooLong) {
    if (!imports.contextUpgradeHint)
      throw std::logic_error("Context upgrade hint required");
    auto display = std::string("Context limit reached · /compact or /clear to continue");
    const auto hint = imports.contextUpgradeHint();
    if (!hint.empty()) display += " · " + hint;
    return card(Kind::ContextLimit, std::move(display));
  }
  if (text == protocol.creditBalanceLow)
    return card(Kind::CreditBalance, protocol.creditBalanceDisplay);
  if (text == protocol.invalidKey) {
    if (!imports.isKeychainLocked)
      throw std::logic_error("Keychain state required");
    auto display = protocol.invalidKey;
    if (imports.isKeychainLocked())
      display += "\n· Run in another terminal: security unlock-keychain";
    return card(Kind::InvalidKey, std::move(display));
  }
  if (text == protocol.externalInvalidKey)
    return card(Kind::ExternalInvalidKey, text);
  if (text == protocol.organizationDisabled ||
      text == protocol.organizationDisabledWithAuth)
    return card(Kind::OrganizationDisabled, text);
  if (text == protocol.tokenRevoked) return card(Kind::TokenRevoked, text);
  if (text == protocol.apiTimeout) {
    auto display = text;
    if (imports.apiTimeoutEnvironment && !imports.apiTimeoutEnvironment->empty())
      display += " (API_TIMEOUT_MS=" + *imports.apiTimeoutEnvironment +
                 "ms, try increasing it)";
    return card(Kind::Timeout, std::move(display));
  }
  if (text == protocol.modelOffSwitch) {
    if (!imports.fallbackModelName)
      throw std::logic_error("Fallback model name required");
    return card(Kind::ModelUnavailable,
      "The selected model is unavailable.\nTo continue immediately, use /model to switch to " +
      imports.fallbackModelName() + " and continue coding.");
  }
  if (text == protocol.userAbort)
    return card(Kind::Interrupted,
                "Interrupted · What should Batbox do instead?");
  if (!imports.startsWithApiErrorPrefix)
    throw std::logic_error("API error classification required");
  if (imports.startsWithApiErrorPrefix(text)) {
    const auto units = value::utf16CodeUnits(text);
    const bool truncated = !verbose && units.size() > 1000;
    const auto display = text == protocol.apiErrorPrefix ?
      protocol.apiErrorPrefix + ": Please wait a moment and try again." :
      truncated ? value::fromUtf16(units.substr(0, 1000)) + "…" : text;
    return card(Kind::ApiError, display, truncated);
  }
  return card(Kind::Markdown, text);
}

std::string kindName(Kind kind) {
  switch (kind) {
    case Kind::Hidden: return "hidden";
    case Kind::RateLimit: return "rate_limit";
    case Kind::ContextLimit: return "context_limit";
    case Kind::CreditBalance: return "credit_balance";
    case Kind::InvalidKey: return "invalid_key";
    case Kind::ExternalInvalidKey: return "external_invalid_key";
    case Kind::OrganizationDisabled: return "organization_disabled";
    case Kind::TokenRevoked: return "token_revoked";
    case Kind::Timeout: return "timeout";
    case Kind::ModelUnavailable: return "model_unavailable";
    case Kind::Interrupted: return "interrupted";
    case Kind::ApiError: return "api_error";
    case Kind::Markdown: return "markdown";
  }
  throw std::logic_error("Unknown assistant card kind");
}
} // namespace batbox::interactive::cards_assistant
