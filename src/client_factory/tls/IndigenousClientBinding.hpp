#pragma once
#include "transport/Client.hpp"
#include <functional>
#include <string>

namespace batbox::client_factory::tls {
std::function<void(transport::ClientConfig&)> makeIndigenousTlsClientConfigurator(
  std::function<void(const std::string&,const std::string&)> log={});
}
