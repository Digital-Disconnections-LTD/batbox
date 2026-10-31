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
} // namespace batbox::transport
