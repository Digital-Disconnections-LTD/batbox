#include "api_errors/Internal.hpp"

namespace batbox::api_errors {
Json createSystemAPIErrorMessage(const retry::ErrorRecord& error,double retryInMs,
    double retryAttempt,double maxRetries,Providers& providers){
  internal::require(providers.nowISO,"nowISO");
  internal::require(providers.randomUUID,"randomUUID");
  Json result={{"type","system"},{"subtype","api_error"},{"level","error"}};
  if(error.cause)result["cause"]=retry::errorToJson(*error.cause);
  result["error"]=retry::errorToJson(error);
  result["retryInMs"]=retryInMs;
  result["retryAttempt"]=retryAttempt;
  result["maxRetries"]=maxRetries;
  result["timestamp"]=providers.nowISO();
  result["uuid"]=providers.randomUUID();
  return result;
}
std::optional<Json> getErrorMessageIfRefusal(const std::optional<std::string>& stopReason,
    const std::string& model,Providers& providers){
  if(stopReason!="refusal")return std::nullopt;
  const std::string policy=internal::required(providers.policyURL,"policyURL");
  const bool nonInteractive=internal::nonInteractive(providers);
  const std::string baseMessage=nonInteractive
    ? "API Error: Batbox is unable to respond to this request, which appears to violate our Usage Policy ("+policy+"). Try rephrasing the request or attempting a different approach."
    : "API Error: Batbox is unable to respond to this request, which appears to violate our Usage Policy ("+policy+"). Please double press esc to edit your last message or start a new session for Batbox to assist with a different task.";
  const std::string preferred=internal::required(providers.preferredModel,"preferredModel");
  const std::string suggestion=model!=preferred
    ? " If you are seeing this refusal repeatedly, try running /model "+preferred+" to switch models."
    : "";
  return internal::assistantError(baseMessage+suggestion,"invalid_request",providers);
}
} // namespace batbox::api_errors
