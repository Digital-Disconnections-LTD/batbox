#pragma once
#include <optional>
#include <string>
#include <vector>

namespace batbox::client_factory::tls {
std::string indigenousCaBundlePath();
#ifdef _WIN32
std::optional<std::vector<std::string>> indigenousWindowsRootCertificates();
#endif
}
