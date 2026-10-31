#pragma once
#include <functional>
#include <optional>
#include <string>

namespace batbox::cli::remote {
using ProxyEnvironment=std::function<std::optional<std::string>(
  const std::string&)>;
std::optional<std::string> getWebSocketProxyUrl(
  const std::string& url,const ProxyEnvironment& environment);
bool shouldBypassWebSocketProxy(const std::string& url,
  const std::optional<std::string>& noProxy);
}
