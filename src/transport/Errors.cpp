#include "transport/Errors.hpp"
namespace batbox::transport {
namespace {
std::string makeMessage(std::optional<long> status, const Json& body, const std::string& fallback) {
  std::string message;
  const auto& field = value::property(body,"message");
  if (value::truthy(field)) message = field.is_string() ? field.get<std::string>() : value::stringify(field);
  else if (value::truthy(body)) message = value::stringify(body);
  else message = fallback;
  if (status && *status != 0) return std::to_string(*status) + (message.empty() ? " status code (no body)" : " " + message);
  return message.empty() ? "(no status code or body)" : message;
}
} // namespace
APIError::APIError(std::optional<long> code, Json value, Json fields, std::string message)
  : std::runtime_error(makeMessage(code,value,message)), status(code), body(std::move(value)), headers(std::move(fields)) {
  const auto& id = value::property(headers,"request-id");
  if (id.is_string()) requestId = id.get<std::string>();
}
APIConnectionError::APIConnectionError(std::string message, std::string errorCode)
  : APIError({},nullptr,Json::object(),std::move(message)), code(std::move(errorCode)) {}
APIConnectionTimeoutError::APIConnectionTimeoutError(std::string message)
  : APIConnectionError(std::move(message),"ETIMEDOUT") {}
APIUserAbortError::APIUserAbortError() : APIError({},nullptr,Json::object(),"Request was aborted.") {}
APIError redirectError(long status, const Json& headers, const std::string& path) {
  const auto& location = value::property(headers,"location");
  if (!location.is_string() || location.get<std::string>().empty())
    return APIError(status,nullptr,headers,
      "redirect with no Location; Batbox does not follow redirects on model requests. "
      "Check this endpoint's base URL");
  const auto target = location.get<std::string>();
  std::string message = "redirect to " + target + "; Batbox does not follow redirects on "
    "model requests, so your key is only sent to the address you configured. ";
  auto base = target.substr(0,target.find_first_of("?#"));
  if (target.find("://") != std::string::npos && !path.empty() && base.ends_with(path)) {
    base.resize(base.size()-path.size());
    message += "Set this endpoint's base URL to " + base;
  } else message += "Set this endpoint's base URL to the address it points to";
  return APIError(status,nullptr,headers,message);
}
} // namespace batbox::transport
