#include "transport/RawHttp.hpp"
#include "transport/Messages.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <cerrno>
#include <exception>
#include <set>
#include <sstream>
#include <utility>

namespace batbox::transport {
struct HttpSession::Handle {
  CURL* curl;
  Handle() {
    static const auto initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (initialized != CURLE_OK) throw APIConnectionError("HTTP initialization failed");
    curl = curl_easy_init();
    if (!curl) throw APIConnectionError("Cannot create HTTP client");
  }
  ~Handle() { curl_easy_cleanup(curl); }
};
namespace {
using Clock = std::chrono::steady_clock;
struct HeaderList {
  curl_slist* list = nullptr;
  ~HeaderList() { curl_slist_free_all(list); }
  void add(const std::string& value) {
    auto* next = curl_slist_append(list,value.c_str());
    if (!next) throw std::bad_alloc();
    list = next;
  }
};
struct State {
  const ClientConfig& config;
  bool stream;
  const std::function<void(std::string_view)>& onData;
  std::stop_token stop;
  const std::function<void(const Response&)>& onResponse;
  RawResponse response;
  std::exception_ptr callbackError;
  Clock::time_point lastActivity = Clock::now();
  bool idleTimeout = false;
  bool headersReported = false;
};
std::string lowerAscii(std::string text) {
  std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c) {
    return static_cast<char>(c >= 'A' && c <= 'Z' ? c+('a'-'A') : c); });
  return text;
}
std::size_t onHeader(char* buffer, std::size_t size, std::size_t count, void* context) {
  auto& state = *static_cast<State*>(context);
  const auto bytes = size*count;
  try {
    state.lastActivity = Clock::now();
    const std::string line(buffer,bytes);
    if (line.starts_with("HTTP/")) {
      std::istringstream status(line);
      std::string version;
      status >> version >> state.response.status;
      state.response.headers = Json::object();
      state.headersReported = false;
    } else {
      const auto colon = line.find(':');
      if (colon != std::string::npos) {
        const auto name = lowerAscii(value::trim(line.substr(0,colon)));
        const auto text = value::trim(line.substr(colon+1));
        auto& headers = state.response.headers;
        if (headers.contains(name)) headers[name] = headers[name].get<std::string>() + ", " + text;
        else headers[name] = text;
      } else if (value::trim(line).empty() && state.response.status >= 200 &&
                 !(state.response.status >= 300 && state.response.status < 400) && !state.headersReported) {
        state.headersReported = true;
        if (state.onResponse) state.onResponse(Response{state.response.status,state.response.headers,nullptr});
      }
    }
    return bytes;
  } catch (...) { state.callbackError = std::current_exception(); return 0; }
}
std::size_t onWrite(char* bytes, std::size_t size, std::size_t count, void* context) {
  auto& state = *static_cast<State*>(context);
  const auto length = size*count;
  try {
    state.lastActivity = Clock::now();
    if (state.stream && state.onData && state.response.status >= 200 && state.response.status < 300)
      state.onData(std::string_view(bytes,length));
    else state.response.body.append(bytes,length);
    return length;
  } catch (...) { state.callbackError = std::current_exception(); return 0; }
}
int onProgress(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
  auto& state = *static_cast<State*>(context);
  if (state.stop.stop_requested()) return 1;
  if (state.stream && state.config.streamIdleTimeout.count() > 0 &&
      Clock::now()-state.lastActivity >= state.config.streamIdleTimeout) {
    state.idleTimeout = true;
    return 1;
  }
  return 0;
}
std::string connectionCode(CURL* http) {
  long number = 0;
  curl_easy_getinfo(http,CURLINFO_OS_ERRNO,&number);
  switch (number) {
    case EPIPE: return "EPIPE";
    case ECONNRESET: return "ECONNRESET";
    case ECONNREFUSED: return "ECONNREFUSED";
    case ETIMEDOUT: return "ETIMEDOUT";
    default: return {};
  }
}
std::string originOf(const std::string& url) {
  const auto scheme = url.find("://");
  if (scheme == std::string::npos) return url;
  auto end = url.find('/',scheme+3);
  auto origin = url.substr(0,end);
  if (const auto at = origin.find('@',scheme+3); at != std::string::npos)
    origin = origin.substr(0,scheme+3)+origin.substr(at+1);
  return origin;
}
} // namespace

HttpSession::HttpSession(ClientConfig config) : handle_(std::make_unique<Handle>()), config_(std::move(config)) {}
HttpSession::~HttpSession() = default;

RawResponse HttpSession::perform(const RawRequest& request,
    const std::function<void(std::string_view)>& onData, std::stop_token stop,
    const std::function<void(const Response&)>& onResponse,
    std::optional<std::chrono::milliseconds> timeout) {
  std::lock_guard lock(mutex_);
  if (stop.stop_requested()) throw APIUserAbortError();
  HeaderList headers;
  for (auto it = request.headers.begin(); it != request.headers.end(); ++it) {
    if (it.value().is_null()) headers.add(it.key()+":");
    else headers.add(it.key()+(it.value()=="" ? ";" : ": "+it.value().get<std::string>()));
  }
  State state{config_,request.stream,onData,stop,onResponse,{},{},Clock::now(),false,false};
  CURL* http = handle_->curl;
  curl_easy_reset(http);
  curl_easy_setopt(http,CURLOPT_URL,request.url.c_str());
  if (request.method == "GET") curl_easy_setopt(http,CURLOPT_HTTPGET,1L);
  else {
    curl_easy_setopt(http,CURLOPT_POST,1L);
    curl_easy_setopt(http,CURLOPT_POSTFIELDS,request.body.data());
    curl_easy_setopt(http,CURLOPT_POSTFIELDSIZE_LARGE,static_cast<curl_off_t>(request.body.size()));
  }
  curl_easy_setopt(http,CURLOPT_HTTPHEADER,headers.list);
  curl_easy_setopt(http,CURLOPT_NOSIGNAL,1L);
  curl_easy_setopt(http,CURLOPT_TCP_KEEPALIVE,config_.disableKeepAlive?0L:1L);
  curl_easy_setopt(http,CURLOPT_FORBID_REUSE,config_.disableKeepAlive?1L:0L);
  if (!config_.proxyUrl.empty()) curl_easy_setopt(http,CURLOPT_PROXY,config_.proxyUrl.c_str());
  if (!config_.unixSocketPath.empty())
    curl_easy_setopt(http,CURLOPT_UNIX_SOCKET_PATH,config_.unixSocketPath.c_str());
  curl_blob certificate{const_cast<char*>(config_.tlsCertificatePem.data()),
    config_.tlsCertificatePem.size(),CURL_BLOB_NOCOPY};
  curl_blob privateKey{const_cast<char*>(config_.tlsPrivateKeyPem.data()),
    config_.tlsPrivateKeyPem.size(),CURL_BLOB_NOCOPY};
  if (!config_.tlsCertificatePem.empty()) {
    curl_easy_setopt(http,CURLOPT_SSLCERTTYPE,"PEM");
    curl_easy_setopt(http,CURLOPT_SSLCERT_BLOB,&certificate);
    curl_easy_setopt(http,CURLOPT_PROXY_SSLCERTTYPE,"PEM");
    curl_easy_setopt(http,CURLOPT_PROXY_SSLCERT_BLOB,&certificate);
  }
  if (!config_.tlsPrivateKeyPem.empty()) {
    curl_easy_setopt(http,CURLOPT_SSLKEYTYPE,"PEM");
    curl_easy_setopt(http,CURLOPT_SSLKEY_BLOB,&privateKey);
    curl_easy_setopt(http,CURLOPT_PROXY_SSLKEYTYPE,"PEM");
    curl_easy_setopt(http,CURLOPT_PROXY_SSLKEY_BLOB,&privateKey);
  }
  if (!config_.tlsKeyPassphrase.empty()) {
    curl_easy_setopt(http,CURLOPT_KEYPASSWD,config_.tlsKeyPassphrase.c_str());
    curl_easy_setopt(http,CURLOPT_PROXY_KEYPASSWD,config_.tlsKeyPassphrase.c_str());
  }
  std::string caBundle;
  for (const auto& pem : config_.tlsCaPem) {
    caBundle += pem;
    if (!pem.empty() && pem.back() != '\n') caBundle += '\n';
  }
  curl_blob ca{caBundle.data(),caBundle.size(),CURL_BLOB_NOCOPY};
  if (!caBundle.empty()) {
    curl_easy_setopt(http,CURLOPT_CAINFO_BLOB,&ca);
    curl_easy_setopt(http,CURLOPT_PROXY_CAINFO_BLOB,&ca);
  }
  curl_easy_setopt(http,CURLOPT_ACCEPT_ENCODING,"");
  curl_easy_setopt(http,CURLOPT_FOLLOWLOCATION,1L);
  curl_easy_setopt(http,CURLOPT_MAXREDIRS,20L);
  curl_easy_setopt(http,CURLOPT_CONNECTTIMEOUT_MS,static_cast<long>(config_.connectTimeout.count()));
  curl_easy_setopt(http,CURLOPT_TIMEOUT_MS,static_cast<long>(timeout.value_or(config_.requestTimeout).count()));
  curl_easy_setopt(http,CURLOPT_HEADERFUNCTION,onHeader);
  curl_easy_setopt(http,CURLOPT_HEADERDATA,&state);
  curl_easy_setopt(http,CURLOPT_WRITEFUNCTION,onWrite);
  curl_easy_setopt(http,CURLOPT_WRITEDATA,&state);
  curl_easy_setopt(http,CURLOPT_NOPROGRESS,0L);
  curl_easy_setopt(http,CURLOPT_XFERINFOFUNCTION,onProgress);
  curl_easy_setopt(http,CURLOPT_XFERINFODATA,&state);
  const auto result = curl_easy_perform(http);
  if (state.callbackError) std::rethrow_exception(state.callbackError);
  if (stop.stop_requested()) throw APIUserAbortError();
  if (state.idleTimeout) throw APIConnectionTimeoutError("Stream idle timeout - no chunks received");
  if (result == CURLE_OPERATION_TIMEDOUT) throw APIConnectionTimeoutError();
  if (result != CURLE_OK) throw APIConnectionError(curl_easy_strerror(result),connectionCode(http));
  return std::move(state.response);
}

Json requestHeaders(const ClientConfig& config, bool stream) {
  Json headers = messageHeaders(Json::object(),config.headers);
  if (!headers.contains("content-type")) headers["content-type"] = "application/json";
  if (!headers.contains("accept")) headers["accept"] = stream ? "text/event-stream" : "application/json";
  if (!headers.contains("user-agent")) headers["user-agent"] = "batbox/0.0.1";
  if (!config.authToken.empty()) headers["authorization"] = "Bearer " + config.authToken;
  return headers;
}

std::string joinUrl(const std::string& baseUrl, const std::string& path) {
  auto url = baseUrl;
  while (url.ends_with('/')) url.pop_back();
  return url + path;
}

std::string errorText(const Json& body, const std::string& raw) {
  const auto& nested = value::property(value::property(body,"error"),"message");
  if (nested.is_string() && !nested.get<std::string>().empty()) return nested.get<std::string>();
  const auto& error = value::property(body,"error");
  if (error.is_string() && !error.get<std::string>().empty()) return error.get<std::string>();
  const auto& message = value::property(body,"message");
  if (message.is_string() && !message.get<std::string>().empty()) return message.get<std::string>();
  const auto& detail = value::property(body,"detail");
  if (detail.is_string() && !detail.get<std::string>().empty()) return detail.get<std::string>();
  auto text = value::trim(raw);
  if (text.size() > 300) text = text.substr(0,300) + "...";
  return text;
}

APIError readableAuthError(const APIError& error, const std::string& baseUrl) {
  if (!error.status || (*error.status != 401 && *error.status != 403)) return error;
  std::string message = "from " + originOf(baseUrl) + ": check auth_token";
  const auto detail = errorText(error.body,{});
  if (!detail.empty()) message += " · " + detail;
  APIError readable(error.status,nullptr,error.headers,message);
  readable.body = error.body;
  return readable;
}

Json fetchModelList(HttpSession& session, const std::string& url) {
  Json ids = Json::array();
  std::set<std::string> seen;
  std::string next = url;
  for (int page = 0; page < 50 && !next.empty(); ++page) {
    RawRequest request;
    request.method = "GET";
    request.url = next;
    request.headers = requestHeaders(session.config(),false);
    request.headers.erase("content-type");
    const auto reply = session.perform(request,{},{},{},std::chrono::seconds(8));
    auto parsed = value::parse(reply.body,false);
    if (parsed.is_discarded()) parsed = nullptr;
    if (reply.status < 200 || reply.status >= 300)
      throw readableAuthError(APIError(reply.status,parsed,reply.headers,reply.body),session.config().baseUrl);
    const auto& data = value::property(parsed,"data");
    if (!data.is_array()) throw APIError(reply.status,parsed,reply.headers,"Model list reply has no data array");
    for (const auto& row : data) {
      const auto& id = value::property(row,"id");
      if (!id.is_string() || id.get<std::string>().empty()) continue;
      if (!seen.insert(id.get<std::string>()).second) continue;
      ids.push_back(Json{{"id",id}});
      if (ids.size() >= 10000) return Json{{"data",ids}};
    }
    next.clear();
    const auto& last = value::property(parsed,"last_id");
    if (value::property(parsed,"has_more") == true && last.is_string() && !last.get<std::string>().empty()) {
      next = url + (url.find('?') == std::string::npos ? "?" : "&") + "after_id=" + last.get<std::string>();
    }
  }
  return Json{{"data",ids}};
}
} // namespace batbox::transport
