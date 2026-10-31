#include "retry/Internal.hpp"
#include <set>

namespace batbox::retry {
namespace {
const std::set<std::string> SSL_ERROR_CODES = {
  "UNABLE_TO_VERIFY_LEAF_SIGNATURE", "UNABLE_TO_GET_ISSUER_CERT",
  "UNABLE_TO_GET_ISSUER_CERT_LOCALLY", "CERT_SIGNATURE_FAILURE",
  "CERT_NOT_YET_VALID", "CERT_HAS_EXPIRED", "CERT_REVOKED", "CERT_REJECTED",
  "CERT_UNTRUSTED", "DEPTH_ZERO_SELF_SIGNED_CERT", "SELF_SIGNED_CERT_IN_CHAIN",
  "CERT_CHAIN_TOO_LONG", "PATH_LENGTH_EXCEEDED", "ERR_TLS_CERT_ALTNAME_INVALID",
  "HOSTNAME_MISMATCH", "ERR_TLS_HANDSHAKE_TIMEOUT", "ERR_SSL_WRONG_VERSION_NUMBER",
  "ERR_SSL_DECRYPTION_FAILED_OR_BAD_RECORD_MAC",
};
}
std::optional<ConnectionErrorDetails> extractConnectionErrorDetails(const ErrorRecord& error) {
  const ErrorRecord* current = &error;
  int depth = 0;
  while (current && depth < 5) {
    if (!current->code.empty()) return ConnectionErrorDetails{
      current->code, current->message, SSL_ERROR_CODES.count(current->code) != 0};
    if (current->cause && current->cause.get() != current) {
      current = current->cause.get(); ++depth;
    } else break;
  }
  return std::nullopt;
}
Json errorToJson(const ErrorRecord& error) {
  Json out = {{"kind", error.kind == ErrorRecord::Kind::Api ? "api" :
      error.kind == ErrorRecord::Kind::Connection ? "connection" :
      error.kind == ErrorRecord::Kind::Abort ? "abort" : "other"},
      {"message", error.message}, {"name", error.name}};
  if (error.status) out["status"] = *error.status;
  if (!error.body.empty()) out["body"] = error.body;
  if (!error.headers.empty()) out["headers"] = error.headers;
  if (!error.legacyHeaders.empty()) out["legacyHeaders"] = error.legacyHeaders;
  if (!error.code.empty()) out["code"] = error.code;
  if (error.cause) out["cause"] = errorToJson(*error.cause);
  return out;
}
ErrorRecord errorFromJson(const Json& value) {
  ErrorRecord out;
  const std::string kind = internal::stringField(value, "kind");
  out.kind = kind == "api" ? ErrorRecord::Kind::Api :
    kind == "connection" ? ErrorRecord::Kind::Connection :
    kind == "abort" ? ErrorRecord::Kind::Abort : ErrorRecord::Kind::Other;
  Json status = internal::field(value, "status");
  if (status.is_number_integer()) out.status = status.get<long>();
  if (internal::has(value,"body")) out.body=value.at("body");
  if (internal::has(value,"headers")) out.headers=value.at("headers");
  if (internal::has(value,"legacyHeaders")) out.legacyHeaders=value.at("legacyHeaders");
  out.message=internal::stringField(value,"message");
  out.code=internal::stringField(value,"code");
  out.name=internal::has(value,"name") ? internal::stringField(value,"name") : "Error";
  if (internal::has(value,"cause") && value.at("cause").is_object())
    out.cause=std::make_shared<ErrorRecord>(errorFromJson(value.at("cause")));
  return out;
}
Json contextToJson(const RetryContext& context) {
  Json out = {{"model", context.model}, {"thinkingConfig", context.thinkingConfig}};
  if (context.maxTokensOverride) out["maxTokensOverride"] = *context.maxTokensOverride;
  if (context.fastMode) out["fastMode"] = *context.fastMode;
  return out;
}
CannotRetryError::CannotRetryError(std::optional<ErrorRecord> original, RetryContext context)
  : std::runtime_error(original ? original->message : "undefined"),
    originalError(std::move(original)), retryContext(std::move(context)) {}
FallbackTriggeredError::FallbackTriggeredError(std::string original, std::string fallback)
  : std::runtime_error("Model fallback triggered: " + original + " -> " + fallback),
    originalModel(std::move(original)), fallbackModel(std::move(fallback)) {}
} // namespace batbox::retry
