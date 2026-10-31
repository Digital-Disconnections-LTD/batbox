#include "transport/Client.hpp"
#include "transport/Errors.hpp"
#include "transport/SSE.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <cerrno>
#include <exception>
#include <sstream>
#include <utility>

namespace batbox::transport {
struct Client::Indigenous {
  CURL* handle;
  Indigenous() {
    static const auto initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (initialized != CURLE_OK) throw APIConnectionError("HTTP initialization failed");
    handle = curl_easy_init();
    if (!handle) throw APIConnectionError("Cannot create HTTP client");
  }
  ~Indigenous() { curl_easy_cleanup(handle); }
};
namespace {
using Clock = std::chrono::steady_clock;
struct Headers {
  curl_slist* list = nullptr;
  ~Headers() { curl_slist_free_all(list); }
  void add(const std::string& value) {
    auto* next = curl_slist_append(list,value.c_str());
    if (!next) throw std::bad_alloc();
    list = next;
  }
};
struct State {
  const ClientConfig& config;
  bool stream;
  const std::function<void(const Json&)>& onEvent;
  std::stop_token stop;
  const std::function<void(const Response&)>& onResponse;
  Response response;
  std::string rawBody;
  SSEDecoder decoder;
  std::exception_ptr callbackError;
  Clock::time_point lastActivity = Clock::now();
  bool idleTimeout = false;
  bool headersReported = false;
};
std::size_t header(char* buffer, std::size_t size, std::size_t count, void* context) {
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
        auto name = value::trim(line.substr(0,colon));
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c) { return c >= 'A' && c <= 'Z' ? c+('a'-'A') : c; });
        const auto value = value::trim(line.substr(colon+1));
        if (state.response.headers.contains(name)) state.response.headers[name] = state.response.headers[name].get<std::string>() + ", " + value;
        else state.response.headers[name] = value;
      } else if (value::trim(line).empty() && state.response.status >= 200 &&
                 !(state.response.status >= 300 && state.response.status < 400) && !state.headersReported) {
        state.headersReported = true;
        if (state.onResponse) state.onResponse(state.response);
      }
    }
    return bytes;
  } catch (...) { state.callbackError = std::current_exception(); return 0; }
}
std::size_t write(char* bytes, std::size_t size, std::size_t count, void* context) {
  auto& state = *static_cast<State*>(context);
  const auto length = size*count;
  try {
    state.lastActivity = Clock::now();
    if (state.stream && state.response.status >= 200 && state.response.status < 300) {
      for (const auto& event : state.decoder.decode(std::string_view(bytes,length))) dispatchSSE(event,state.response.headers,state.onEvent);
    } else state.rawBody.append(bytes,length);
    return length;
  } catch (...) { state.callbackError = std::current_exception(); return 0; }
}
int progress(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
  auto& state = *static_cast<State*>(context);
  if (state.stop.stop_requested()) return 1;
  if (state.stream && state.config.streamIdleTimeout.count() > 0 && Clock::now()-state.lastActivity >= state.config.streamIdleTimeout) {
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
} // namespace
Client::Client(ClientConfig settings) : indigenous(std::make_unique<Indigenous>()), config(std::move(settings)) {}
Client::~Client() = default;
Response Client::request(const Json& body, bool stream, const std::function<void(const Json&)>& onEvent, std::stop_token stop,
                         const std::function<void(const Response&)>& onResponse,
                         std::optional<std::chrono::milliseconds> timeout) {
  std::lock_guard lock(mutex);
  if (stop.stop_requested()) throw APIUserAbortError();
  if (stream && !onEvent) throw std::invalid_argument("Streaming requests require an event consumer");
  auto prepared=prepareMessageRequest(body,config.headers,config.protocol);
  Json request = std::move(prepared.body);
  request["stream"] = stream;
  const auto serialized = value::stringify(request);
  auto url = config.baseUrl;
  while (url.ends_with('/')) url.pop_back();
  url += config.messagesPath;
  Headers headers;
  Json custom = std::move(prepared.headers);
  if (!custom.contains("content-type")) custom["content-type"]="application/json";
  if (!custom.contains("accept")) custom["accept"]=stream ? "text/event-stream" : "application/json";
  if (!custom.contains("user-agent")) custom["user-agent"]="batbox/0.0.1";
  if (!config.authToken.empty()) custom["authorization"] = "Bearer " + config.authToken;
  for (auto it = custom.begin(); it != custom.end(); ++it) {
    if (it.value().is_null()) headers.add(it.key()+":");
    else headers.add(it.key()+(it.value()=="" ? ";" : ": "+it.value().get<std::string>()));
  }
  State state{config,stream,onEvent,stop,onResponse,{}, {},{}, {},Clock::now(),false,false};
  CURL* http = indigenous->handle;
  curl_easy_reset(http);
  curl_easy_setopt(http,CURLOPT_URL,url.c_str());
  curl_easy_setopt(http,CURLOPT_POST,1L);
  curl_easy_setopt(http,CURLOPT_POSTFIELDS,serialized.data());
  curl_easy_setopt(http,CURLOPT_POSTFIELDSIZE_LARGE,static_cast<curl_off_t>(serialized.size()));
  curl_easy_setopt(http,CURLOPT_HTTPHEADER,headers.list);
  curl_easy_setopt(http,CURLOPT_NOSIGNAL,1L);
  curl_easy_setopt(http,CURLOPT_TCP_KEEPALIVE,
    config.disableKeepAlive?0L:1L);
  curl_easy_setopt(http,CURLOPT_FORBID_REUSE,
    config.disableKeepAlive?1L:0L);
  if(!config.proxyUrl.empty())
    curl_easy_setopt(http,CURLOPT_PROXY,config.proxyUrl.c_str());
  if(!config.unixSocketPath.empty())
    curl_easy_setopt(http,CURLOPT_UNIX_SOCKET_PATH,
      config.unixSocketPath.c_str());
  curl_blob certificate{const_cast<char*>(config.tlsCertificatePem.data()),
    config.tlsCertificatePem.size(),CURL_BLOB_NOCOPY};
  curl_blob privateKey{const_cast<char*>(config.tlsPrivateKeyPem.data()),
    config.tlsPrivateKeyPem.size(),CURL_BLOB_NOCOPY};
  if(!config.tlsCertificatePem.empty()){
    curl_easy_setopt(http,CURLOPT_SSLCERTTYPE,"PEM");
    curl_easy_setopt(http,CURLOPT_SSLCERT_BLOB,&certificate);
    curl_easy_setopt(http,CURLOPT_PROXY_SSLCERTTYPE,"PEM");
    curl_easy_setopt(http,CURLOPT_PROXY_SSLCERT_BLOB,&certificate);
  }
  if(!config.tlsPrivateKeyPem.empty()){
    curl_easy_setopt(http,CURLOPT_SSLKEYTYPE,"PEM");
    curl_easy_setopt(http,CURLOPT_SSLKEY_BLOB,&privateKey);
    curl_easy_setopt(http,CURLOPT_PROXY_SSLKEYTYPE,"PEM");
    curl_easy_setopt(http,CURLOPT_PROXY_SSLKEY_BLOB,&privateKey);
  }
  if(!config.tlsKeyPassphrase.empty()){
    curl_easy_setopt(http,CURLOPT_KEYPASSWD,
      config.tlsKeyPassphrase.c_str());
    curl_easy_setopt(http,CURLOPT_PROXY_KEYPASSWD,
      config.tlsKeyPassphrase.c_str());
  }
  std::string caBundle;
  for(const auto& pem:config.tlsCaPem){
    caBundle+=pem;
    if(!pem.empty()&&pem.back()!='\n')caBundle+='\n';
  }
  curl_blob ca{caBundle.data(),caBundle.size(),CURL_BLOB_NOCOPY};
  if(!caBundle.empty()){
    curl_easy_setopt(http,CURLOPT_CAINFO_BLOB,&ca);
    curl_easy_setopt(http,CURLOPT_PROXY_CAINFO_BLOB,&ca);
  }
  curl_easy_setopt(http,CURLOPT_ACCEPT_ENCODING,"");
  curl_easy_setopt(http,CURLOPT_FOLLOWLOCATION,1L);
  curl_easy_setopt(http,CURLOPT_MAXREDIRS,20L);
  curl_easy_setopt(http,CURLOPT_CONNECTTIMEOUT_MS,static_cast<long>(config.connectTimeout.count()));
  curl_easy_setopt(http,CURLOPT_TIMEOUT_MS,static_cast<long>(timeout.value_or(config.requestTimeout).count()));
  curl_easy_setopt(http,CURLOPT_HEADERFUNCTION,header);
  curl_easy_setopt(http,CURLOPT_HEADERDATA,&state);
  curl_easy_setopt(http,CURLOPT_WRITEFUNCTION,write);
  curl_easy_setopt(http,CURLOPT_WRITEDATA,&state);
  curl_easy_setopt(http,CURLOPT_NOPROGRESS,0L);
  curl_easy_setopt(http,CURLOPT_XFERINFOFUNCTION,progress);
  curl_easy_setopt(http,CURLOPT_XFERINFODATA,&state);
  const auto result = curl_easy_perform(http);
  if (state.callbackError) std::rethrow_exception(state.callbackError);
  if (stop.stop_requested()) throw APIUserAbortError();
  if (state.idleTimeout) throw APIConnectionTimeoutError("Stream idle timeout - no chunks received");
  if (result == CURLE_OPERATION_TIMEDOUT) throw APIConnectionTimeoutError();
  if (result != CURLE_OK) throw APIConnectionError(curl_easy_strerror(result),connectionCode(http));
  if (state.response.status < 200 || state.response.status >= 300) {
    auto parsed = value::parse(state.rawBody,false);
    if (parsed.is_discarded()) parsed = nullptr;
    throw APIError(state.response.status,std::move(parsed),std::move(state.response.headers),state.rawBody);
  }
  if (stream) {
    for (const auto& event : state.decoder.flush()) dispatchSSE(event,state.response.headers,onEvent);
  } else if (!state.rawBody.empty()) state.response.body = value::parse(state.rawBody);
  return state.response;
}
} // namespace batbox::transport
