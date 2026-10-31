#pragma once
#include "client_factory/tls/TlsOptions.hpp"

namespace batbox::client_factory::tls {
// An empty path selects the packaged/indigenous libcurl CA bundle lazily, only
// when certificate resolution requests a base store.
Services makeIndigenousTlsServices(const std::string& caBundlePath={},
  std::function<void(const std::string&,const std::string&)> log={});
}
