#pragma once
#include "runtime/JsonValue.hpp"
#include "transport/Client.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace batbox::providers::cloud {
struct HttpRequest {
  std::string method = "POST";
  std::string url;
  std::vector<std::pair<std::string,std::string>> headers;
  std::string body;
  std::optional<std::chrono::milliseconds> timeout;
  bool stream = false;
};
struct HttpResult {
  long status = 0;
  Json headers = Json::object();
  // The body of a non-streamed exchange, or of any response outside 2xx.
  std::string body;
};
using OnHeaders = std::function<void(long status, const Json& headers)>;
using OnBytes = std::function<void(std::string_view bytes)>;
// One reusable libcurl handle with the network settings of a transport::ClientConfig.
// It reports HTTP failures as results; transport failures throw transport errors.
class HttpExchange {
  struct Handle;
  std::unique_ptr<Handle> handle;
  transport::ClientConfig network;
public:
  explicit HttpExchange(transport::ClientConfig network = {});
  ~HttpExchange();
  HttpExchange(const HttpExchange&) = delete;
  HttpExchange& operator=(const HttpExchange&) = delete;
  // For 2xx streamed requests, body bytes go to onBytes as they arrive.
  HttpResult perform(const HttpRequest& request, std::stop_token stop = {},
                     const OnHeaders& onHeaders = {}, const OnBytes& onBytes = {});
  const transport::ClientConfig& settings() const { return network; }
};
struct UrlParts {
  std::string scheme;
  std::string host;     // host[:port] as it belongs in the Host header
  std::string target;   // path and query
};
UrlParts splitUrl(const std::string& url);
std::string joinUrl(std::string base, std::string_view path);
std::string percentEncodeComponent(std::string_view text);
std::string formEncode(const std::vector<std::pair<std::string,std::string>>& fields);
} // namespace batbox::providers::cloud
