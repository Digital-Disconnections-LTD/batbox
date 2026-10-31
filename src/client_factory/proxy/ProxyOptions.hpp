#pragma once
#include "client_factory/ClientFactory.hpp"
#include <atomic>
#include <functional>
#include <optional>
#include <string>

namespace batbox::client_factory::proxy {
struct Services {
  std::function<std::optional<std::string>(const std::string&)> environment;
  std::function<NetworkOptions()> getTLSFetchOptions;
  std::function<std::shared_ptr<void>(const std::string&)> getProxyBat;
  bool bunRuntime=true; // indigenous curl has the direct proxy/unix capabilities
  std::string unixSocketKey="BATBOX_UNIX_SOCKET";
};
class ProxyOptions {
public:
  explicit ProxyOptions(Services services);
  void disableKeepAlive();
  void resetKeepAliveForTesting();
  std::optional<std::string> getProxyUrl()const;
  NetworkOptions getProxyFetchOptions(bool forApi=false)const;
private:
  Services services_;
  std::atomic_bool keepAliveDisabled_{false};
  std::optional<std::string> env(const std::string& key)const;
};
}
