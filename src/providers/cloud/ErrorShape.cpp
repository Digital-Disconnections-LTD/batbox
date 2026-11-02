#include "providers/cloud/ErrorShape.hpp"
#include "transport/Errors.hpp"
#include <algorithm>

namespace batbox::providers::cloud {
namespace {
std::string lower(std::string text) {
  std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c) { return c >= 'A' && c <= 'Z' ? c+('a'-'A') : c; });
  return text;
}
std::string stringField(const Json& object, std::initializer_list<const char*> names) {
  if (!object.is_object()) return {};
  for (const auto* name : names)
    if (const auto it = object.find(name); it != object.end() && it->is_string()) return it->get<std::string>();
  return {};
}
} // namespace
std::string errorTypeForStatus(long status) {
  switch (status) {
    case 400: case 422: return "invalid_request_error";
    case 401: return "authentication_error";
    case 403: return "permission_error";
    case 404: return "not_found_error";
    case 413: return "request_too_large";
    case 429: return "rate_limit_error";
    case 503: case 529: return "overloaded_error";
    default: return "api_error";
  }
}
Json errorBody(const std::string& type, const std::string& message) {
  return Json{{"type","error"},{"error",Json{{"type",type},{"message",message}}}};
}
void throwMapped(const MappedError& error, Json headers) {
  auto body = errorBody(error.type,error.message);
  throw transport::APIError(error.status,body,std::move(headers),value::stringify(body));
}
MappedError mapAwsException(const std::string& exceptionType, long httpStatus, const std::string& message) {
  auto name = exceptionType.substr(0,exceptionType.find(':'));
  if (const auto hash = name.rfind('#'); hash != std::string::npos) name = name.substr(hash+1);
  const auto key = lower(name);
  const auto text = (name.empty() ? std::string("Bedrock error") : name)+(message.empty() ? "" : ": "+message);
  if (key == "throttlingexception" || key == "servicequotaexceededexception" || key == "toomanyrequestsexception")
    return {429,"rate_limit_error",text};
  if (key == "modelnotreadyexception") return {429,"overloaded_error",text};
  if (key == "serviceunavailableexception") return {503,"overloaded_error",text};
  if (key == "internalserverexception" || key == "internalfailure" || key == "modelstreamerrorexception" ||
      key == "modelerrorexception") return {500,"api_error",text};
  if (key == "modeltimeoutexception") return {408,"api_error",text};
  if (key == "validationexception") return {400,"invalid_request_error",text};
  if (key == "accessdeniedexception") return {403,"permission_error",text};
  if (key == "resourcenotfoundexception") return {404,"not_found_error",text};
  if (key == "unrecognizedclientexception" || key == "invalidsignatureexception" || key == "expiredtokenexception" ||
      key == "incompletesignatureexception" || key == "missingauthenticationtokenexception")
    return {401,"authentication_error",text};
  const long status = httpStatus >= 400 ? httpStatus : 500;
  return {status,errorTypeForStatus(status),text};
}
void throwGoogleError(long status, const std::string& rawBody, Json headers) {
  auto parsed = value::parse(rawBody,false);
  if (parsed.is_discarded()) parsed = nullptr;
  if (parsed.is_array() && !parsed.empty()) parsed = parsed.front();
  if (parsed.is_object() && parsed.value("type","") == "error" && parsed.contains("error") && parsed["error"].is_object())
    throw transport::APIError(status,parsed,std::move(headers),value::stringify(parsed));
  const auto& error = parsed.is_object() && parsed.contains("error") ? parsed["error"] : Json(nullptr);
  const auto rpc = stringField(error,{"status"});
  std::string message = stringField(error,{"message"});
  if (message.empty()) message = rawBody.empty() ? "Vertex AI request failed" : rawBody.substr(0,2000);
  long mapped = status;
  std::string type = errorTypeForStatus(status);
  if (rpc == "RESOURCE_EXHAUSTED") { mapped = 429; type = "rate_limit_error"; }
  else if (rpc == "UNAVAILABLE") { mapped = 503; type = "overloaded_error"; }
  else if (rpc == "UNAUTHENTICATED") { mapped = 401; type = "authentication_error"; }
  else if (rpc == "PERMISSION_DENIED") { mapped = 403; type = "permission_error"; }
  else if (rpc == "NOT_FOUND") { mapped = 404; type = "not_found_error"; }
  else if (rpc == "INVALID_ARGUMENT" || rpc == "FAILED_PRECONDITION" || rpc == "OUT_OF_RANGE") { mapped = 400; type = "invalid_request_error"; }
  throwMapped({mapped,type,(rpc.empty() ? "" : rpc+": ")+message},std::move(headers));
}
} // namespace batbox::providers::cloud
