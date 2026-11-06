#include "providers/cloud/HttpExchange.hpp"
#include "transport/Errors.hpp"
#include "transport/Perform.hpp"
#include "runtime/Loopback.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <cerrno>
#include <exception>
#include <sstream>
#include <stdexcept>

namespace batbox::providers::cloud {
struct HttpExchange::Handle {
  CURL* curl;
  Handle() {
    static const auto initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (initialized != CURLE_OK) throw transport::APIConnectionError("HTTP initialization failed");
    curl = curl_easy_init();
    if (!curl) throw transport::APIConnectionError("Cannot create HTTP client");
  }
  ~Handle() { curl_easy_cleanup(curl); }
};
namespace {
using Clock = std::chrono::steady_clock;
struct Slist {
  curl_slist* list = nullptr;
  ~Slist() { curl_slist_free_all(list); }
  void add(const std::string& line) {
    auto* next = curl_slist_append(list,line.c_str());
    if (!next) throw std::bad_alloc();
    list = next;
  }
};
struct State {
  const HttpRequest& request;
  const transport::ClientConfig& network;
  std::stop_token stop;
  const OnHeaders& onHeaders;
  const OnBytes& onBytes;
  HttpResult result;
  std::exception_ptr callbackError;
  Clock::time_point lastActivity = Clock::now();
  bool idleTimeout = false;
  bool headersReported = false;
};
bool streaming(const State& state) {
  return state.request.stream && state.onBytes && state.result.status >= 200 && state.result.status < 300;
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
      status >> version >> state.result.status;
      state.result.headers = Json::object();
      state.headersReported = false;
    } else if (const auto colon = line.find(':'); colon != std::string::npos) {
      auto name = value::trim(line.substr(0,colon));
      std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c) { return c >= 'A' && c <= 'Z' ? c+('a'-'A') : c; });
      const auto text = value::trim(line.substr(colon+1));
      auto& headers = state.result.headers;
      if (headers.contains(name)) headers[name] = headers[name].get<std::string>()+", "+text;
      else headers[name] = text;
    } else if (value::trim(line).empty() && state.result.status >= 200 &&
               !(state.result.status >= 300 && state.result.status < 400) && !state.headersReported) {
      state.headersReported = true;
      if (state.onHeaders) state.onHeaders(state.result.status,state.result.headers);
    }
    return bytes;
  } catch (...) { state.callbackError = std::current_exception(); return 0; }
}
std::size_t onWrite(char* bytes, std::size_t size, std::size_t count, void* context) {
  auto& state = *static_cast<State*>(context);
  const auto length = size*count;
  try {
    state.lastActivity = Clock::now();
    if (streaming(state)) state.onBytes(std::string_view(bytes,length));
    else state.result.body.append(bytes,length);
    return length;
  } catch (...) { state.callbackError = std::current_exception(); return 0; }
}
int onProgress(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
  auto& state = *static_cast<State*>(context);
  if (state.stop.stop_requested()) return 1;
  if (state.request.stream && state.network.streamIdleTimeout.count() > 0 &&
      Clock::now()-state.lastActivity >= state.network.streamIdleTimeout) {
    state.idleTimeout = true;
    return 1;
  }
  return 0;
}
std::string connectionCode(CURL* curl) {
  long number = 0;
  curl_easy_getinfo(curl,CURLINFO_OS_ERRNO,&number);
  switch (number) {
    case EPIPE: return "EPIPE";
    case ECONNRESET: return "ECONNRESET";
    case ECONNREFUSED: return "ECONNREFUSED";
    case ETIMEDOUT: return "ETIMEDOUT";
    default: return {};
  }
}
} // namespace
HttpExchange::HttpExchange(transport::ClientConfig settings) : handle(std::make_unique<Handle>()), network(std::move(settings)) {}
HttpExchange::~HttpExchange() = default;
HttpResult HttpExchange::perform(const HttpRequest& request, std::stop_token stop, const OnHeaders& onHeaders, const OnBytes& onBytes) {
  if (stop.stop_requested()) throw transport::APIUserAbortError();
  Slist headers;
  for (const auto& [name,value] : request.headers) headers.add(value.empty() ? name+";" : name+": "+value);
  headers.add("Expect:");
  State state{request,network,stop,onHeaders,onBytes,{},{},Clock::now(),false,false};
  CURL* curl = handle->curl;
  curl_easy_reset(curl);
  curl_easy_setopt(curl,CURLOPT_URL,request.url.c_str());
  if (request.method == "GET") curl_easy_setopt(curl,CURLOPT_HTTPGET,1L);
  else {
    if (request.method != "POST") curl_easy_setopt(curl,CURLOPT_CUSTOMREQUEST,request.method.c_str());
    curl_easy_setopt(curl,CURLOPT_POST,1L);
    curl_easy_setopt(curl,CURLOPT_POSTFIELDS,request.body.data());
    curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE_LARGE,static_cast<curl_off_t>(request.body.size()));
  }
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers.list);
  curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
  curl_easy_setopt(curl,CURLOPT_TCP_KEEPALIVE,network.disableKeepAlive ? 0L : 1L);
  curl_easy_setopt(curl,CURLOPT_FORBID_REUSE,network.disableKeepAlive ? 1L : 0L);
  if (!network.proxyUrl.empty()) curl_easy_setopt(curl,CURLOPT_PROXY,network.proxyUrl.c_str());
  if (runtime::isLoopbackUrl(request.url)) curl_easy_setopt(curl,CURLOPT_NOPROXY,"*");
  if (!network.unixSocketPath.empty()) curl_easy_setopt(curl,CURLOPT_UNIX_SOCKET_PATH,network.unixSocketPath.c_str());
  curl_blob certificate{const_cast<char*>(network.tlsCertificatePem.data()),network.tlsCertificatePem.size(),CURL_BLOB_NOCOPY};
  curl_blob privateKey{const_cast<char*>(network.tlsPrivateKeyPem.data()),network.tlsPrivateKeyPem.size(),CURL_BLOB_NOCOPY};
  if (!network.tlsCertificatePem.empty()) {
    curl_easy_setopt(curl,CURLOPT_SSLCERTTYPE,"PEM");
    curl_easy_setopt(curl,CURLOPT_SSLCERT_BLOB,&certificate);
  }
  if (!network.tlsPrivateKeyPem.empty()) {
    curl_easy_setopt(curl,CURLOPT_SSLKEYTYPE,"PEM");
    curl_easy_setopt(curl,CURLOPT_SSLKEY_BLOB,&privateKey);
  }
  if (!network.tlsKeyPassphrase.empty()) curl_easy_setopt(curl,CURLOPT_KEYPASSWD,network.tlsKeyPassphrase.c_str());
  std::string caBundle;
  for (const auto& pem : network.tlsCaPem) {
    caBundle += pem;
    if (!pem.empty() && pem.back() != '\n') caBundle += '\n';
  }
  curl_blob ca{caBundle.data(),caBundle.size(),CURL_BLOB_NOCOPY};
  if (!caBundle.empty()) {
    curl_easy_setopt(curl,CURLOPT_CAINFO_BLOB,&ca);
    curl_easy_setopt(curl,CURLOPT_PROXY_CAINFO_BLOB,&ca);
  }
  curl_easy_setopt(curl,CURLOPT_ACCEPT_ENCODING,"");
  // Signed requests must not be replayed to another host, so redirects stay off.
  curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
  curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT_MS,static_cast<long>(network.connectTimeout.count()));
  curl_easy_setopt(curl,CURLOPT_TIMEOUT_MS,static_cast<long>(request.timeout.value_or(network.requestTimeout).count()));
  curl_easy_setopt(curl,CURLOPT_HEADERFUNCTION,onHeader);
  curl_easy_setopt(curl,CURLOPT_HEADERDATA,&state);
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,onWrite);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA,&state);
  curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);
  curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,onProgress);
  curl_easy_setopt(curl,CURLOPT_XFERINFODATA,&state);
  const auto code = transport::performStoppable(curl,stop);
  if (state.callbackError) std::rethrow_exception(state.callbackError);
  if (stop.stop_requested()) throw transport::APIUserAbortError();
  if (state.idleTimeout) throw transport::APIConnectionTimeoutError("Stream idle timeout - no chunks received");
  if (code == CURLE_OPERATION_TIMEDOUT) throw transport::APIConnectionTimeoutError();
  if (code != CURLE_OK) throw transport::APIConnectionError(curl_easy_strerror(code),connectionCode(curl));
  return std::move(state.result);
}
UrlParts splitUrl(const std::string& url) {
  const auto schemeEnd = url.find("://");
  if (schemeEnd == std::string::npos) throw std::invalid_argument("Not an absolute URL: "+url);
  UrlParts parts;
  parts.scheme = url.substr(0,schemeEnd);
  const auto hostStart = schemeEnd+3;
  const auto pathStart = url.find_first_of("/?",hostStart);
  parts.host = url.substr(hostStart,pathStart == std::string::npos ? std::string::npos : pathStart-hostStart);
  if (const auto at = parts.host.rfind('@'); at != std::string::npos) parts.host = parts.host.substr(at+1);
  std::transform(parts.host.begin(),parts.host.end(),parts.host.begin(),[](unsigned char c) { return c >= 'A' && c <= 'Z' ? c+('a'-'A') : c; });
  if ((parts.scheme == "https" && parts.host.ends_with(":443")) || (parts.scheme == "http" && parts.host.ends_with(":80")))
    parts.host = parts.host.substr(0,parts.host.rfind(':'));
  parts.target = pathStart == std::string::npos ? "/" : url.substr(pathStart);
  if (parts.target.starts_with('?')) parts.target = "/"+parts.target;
  return parts;
}
std::string joinUrl(std::string base, std::string_view path) {
  while (base.ends_with('/')) base.pop_back();
  if (!path.starts_with('/')) base += '/';
  return base+std::string(path);
}
std::string percentEncodeComponent(std::string_view text) {
  static constexpr char digits[] = "0123456789ABCDEF";
  std::string out;
  for (const unsigned char c : text) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
      out += static_cast<char>(c);
    else { out += '%'; out += digits[c >> 4]; out += digits[c & 15]; }
  }
  return out;
}
std::string formEncode(const std::vector<std::pair<std::string,std::string>>& fields) {
  std::string out;
  for (const auto& [name,value] : fields) out += (out.empty() ? "" : "&")+percentEncodeComponent(name)+"="+percentEncodeComponent(value);
  return out;
}
} // namespace batbox::providers::cloud
