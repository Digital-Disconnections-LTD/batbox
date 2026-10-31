#pragma once
#include "runtime/JsonValue.hpp"
#include <string>

namespace batbox::settings::remote {
Json sortKeysDeep(const Json& value);
std::string computeChecksumFromSettings(const Json& settings);
}
