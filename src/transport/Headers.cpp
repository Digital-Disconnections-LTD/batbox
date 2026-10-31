#include "transport/Client.hpp"
namespace batbox::transport {
Json getCustomHeaders(const std::string& configuredHeaders) {
  Json customHeaders = Json::object();
  if (configuredHeaders.empty()) return customHeaders;
  std::size_t begin = 0;
  while (begin <= configuredHeaders.size()) {
    const auto end = configuredHeaders.find('\n',begin);
    const auto headerString = configuredHeaders.substr(begin,end == std::string::npos ? end : end-begin);
    if (!value::trim(headerString).empty()) {
      const auto colonIdx = headerString.find(':');
      if (colonIdx != std::string::npos) {
        const auto name = value::trim(headerString.substr(0,colonIdx));
        const auto value = value::trim(headerString.substr(colonIdx+1));
        if (!name.empty()) customHeaders[name] = value;
      }
    }
    if (end == std::string::npos) break;
    begin = end+1;
  }
  return customHeaders;
}
void configureApiKeyHeaders(Json& headers, const std::string& authToken, bool isNonInteractiveFlight,
                            const std::function<std::string(bool)>& keyHelper) {
  const auto token = authToken.empty() ? keyHelper(isNonInteractiveFlight) : authToken;
  if (!token.empty()) headers["Authorization"] = "Bearer " + token;
}
} // namespace batbox::transport
