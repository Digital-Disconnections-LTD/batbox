#pragma once
#include "runtime/JsonValue.hpp"
#include "transport/Messages.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <vector>

namespace batbox::transport {
struct ClientConfig {
  std::string baseUrl = "http://127.0.0.1:8000";
  std::string messagesPath = "/v1/messages";
  std::string authToken;
  Json headers = Json::object();
  MessagesProtocol protocol;
  std::chrono::milliseconds requestTimeout = std::chrono::hours(4);
  std::chrono::milliseconds streamIdleTimeout = std::chrono::hours(2);
  std::chrono::milliseconds connectTimeout = std::chrono::seconds(10);
  // Per-client transport overrides.
  std::string proxyUrl;
  std::string unixSocketPath;
  bool disableKeepAlive = false;
  std::string tlsCertificatePem;
  std::string tlsPrivateKeyPem;
  std::string tlsKeyPassphrase;
  std::vector<std::string> tlsCaPem;
};
struct Response {
  long status = 0;
  Json headers = Json::object();
  Json body = nullptr;
};
Json getCustomHeaders(const std::string& configuredHeaders);
void configureApiKeyHeaders(Json& headers, const std::string& authToken,
                            bool isNonInteractiveFlight,
                            const std::function<std::string(bool)>& keyHelper);
class Client {
  struct Indigenous;
  std::unique_ptr<Indigenous> indigenous;
  ClientConfig config;
  std::mutex mutex;
public:
  explicit Client(ClientConfig config);
  ~Client();
  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;
  Response request(const Json& body, bool stream,
                   const std::function<void(const Json&)>& onEvent = {},
                   std::stop_token stop = {},
                   const std::function<void(const Response&)>& onResponse = {},
                   std::optional<std::chrono::milliseconds> timeout = std::nullopt);
};
} // namespace batbox::transport
