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
  std::string modelsPath;
  std::string authToken;
  Json headers = Json::object();
  MessagesProtocol protocol;
  // The endpoint defaults in providers/RequestSettings.hpp; 0 means no limit.
  std::chrono::milliseconds requestTimeout{0};
  std::chrono::milliseconds streamIdleTimeout = std::chrono::hours(6);
  std::chrono::milliseconds connectTimeout = std::chrono::minutes(2);
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
