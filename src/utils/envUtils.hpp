#pragma once
#include "runtime/JsonValue.hpp"
#include <map>
#include <string>
#include <vector>
namespace batbox {
using Environment = std::map<std::string, std::string>;
bool isEnvTruthy(const Json& envVar);
bool isEnvDefinedFalsy(const Json& envVar);
Json parseEnvVars(const std::vector<std::string>& rawEnvArgs = {});
} // namespace batbox
