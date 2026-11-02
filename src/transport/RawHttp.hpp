#pragma once
#include "transport/Client.hpp"
#include "transport/Errors.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

namespace batbox::transport {
struct RawRequest {
  std::string method = "POST"; // POST or GET
  std::string url;
  Json headers = Json::object(); // lowercase name -> text, or null to drop
  std::string body;
  bool stream = false;
};
struct RawResponse {
  long status = 0;
  Json headers = Json::object();
  std::string body; // whole body, except 2xx bytes already given to onData
};
// One reusable connection with the network settings of a ClientConfig
// (timeouts, proxy, TLS). Unlike Client it does not interpret the body, so
// any wire format can sit on top of it. Connection failures, timeouts and
// aborts throw the same errors as Client; HTTP statuses are returned.
class HttpSession {
public:
  explicit HttpSession(ClientConfig config);
  ~HttpSession();
  HttpSession(const HttpSession&) = delete;
  HttpSession& operator=(const HttpSession&) = delete;
  const ClientConfig& config() const { return config_; }
  RawResponse perform(const RawRequest& request,
                      const std::function<void(std::string_view)>& onData = {},
                      std::stop_token stop = {},
                      const std::function<void(const Response&)>& onResponse = {},
                      std::optional<std::chrono::milliseconds> timeout = std::nullopt);
private:
  struct Handle;
  std::unique_ptr<Handle> handle_;
  ClientConfig config_;
  std::mutex mutex_;
};
// Configured headers (lowercased) plus content-type, accept, user-agent and
// the bearer token, the same set Client sends.
Json requestHeaders(const ClientConfig& config, bool stream);
std::string joinUrl(const std::string& baseUrl, const std::string& path);
// GET the model list and return {"data":[{"id":...}]}, following Anthropic
// style has_more/last_id pages. Non-2xx statuses throw APIError.
Json fetchModelList(HttpSession& session, const std::string& url);
// Rewrites a 401/403 so the message names the server and the setting to fix,
// e.g. "401 from http://127.0.0.1:9931: check auth_token · invalid key".
// Status, body and headers are kept.
APIError readableAuthError(const APIError& error, const std::string& baseUrl);
// A message for a non-2xx reply: body.error.message, body.message, or the
// raw text.
std::string errorText(const Json& body, const std::string& raw);
} // namespace batbox::transport
