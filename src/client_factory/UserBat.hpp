#pragma once
#include <string>

namespace batbox::client_factory {
// Builds the product identity for outgoing requests.
// The identity string contains no device or account identifiers.
std::string getBatboxUserBat(const std::string& version);
}
