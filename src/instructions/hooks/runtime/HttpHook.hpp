#pragma once
#include "instructions/hooks/runtime/OutsideRepl.hpp"
#include <map>

namespace batbox::instructions::hooks::runtime {
struct HttpHookPolicy {
  std::optional<std::vector<std::string>> allowedUrls,allowedEnvVars;
};
struct HttpProxy {
  std::string host,protocol;
  int port=0;
};
struct HttpTransportRequest {
  std::string url,body;
  std::map<std::string,std::string> headers;
  std::optional<HttpProxy> sandboxProxy;
  std::optional<std::string> envProxy;
  bool useSsrfGuard=false;
  std::stop_token signal;
};
struct HttpTransportResponse {
  int status=0;
  std::string body;
};
struct HttpHookServices {
  std::function<HttpHookPolicy()> getPolicy;
  std::function<std::optional<std::string>(const std::string&)> getEnv;
  std::function<event_loop::Deferred<std::optional<HttpProxy>>()> getSandboxProxy;
  std::function<std::optional<std::string>()> getProxyUrl;
  std::function<bool(const std::string&)> shouldBypassProxy;
  std::function<event_loop::Deferred<HttpTransportResponse>(HttpTransportRequest)> post;
  std::function<void(const std::string&,const std::string&)> logDebug;
  std::function<std::string(std::exception_ptr)> errorMessage;
};
bool httpUrlMatchesPattern(const std::string& url,const std::string& pattern);
std::string sanitizeHttpHeaderValue(const std::string& value);
std::string interpolateHttpEnvVars(const std::string& value,
  const std::vector<std::string>& allowed,const HttpHookServices& services);
std::function<event_loop::Deferred<HttpOutcome>(HttpRequest)> execHttpHook(
  event_loop::Loop& loop,HttpHookServices services);
} // namespace batbox::instructions::hooks::runtime
