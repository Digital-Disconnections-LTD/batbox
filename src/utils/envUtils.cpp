#include "utils/envUtils.hpp"
#include <algorithm>
#include <stdexcept>
namespace batbox {
bool isEnvTruthy(const Json& envVar) {
  if (!value::truthy(envVar)) return false;
  if (envVar.is_boolean()) return envVar.get<bool>();
  auto normalizedValue = value::trim(envVar.get<std::string>());
  std::transform(normalizedValue.begin(), normalizedValue.end(), normalizedValue.begin(),
    [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; });
  return normalizedValue == "1" || normalizedValue == "true" || normalizedValue == "yes" || normalizedValue == "on";
}
bool isEnvDefinedFalsy(const Json& envVar) {
  if (envVar.is_null()) return false;
  if (envVar.is_boolean()) return !envVar.get<bool>();
  if (!value::truthy(envVar)) return false;
  auto normalizedValue = value::trim(envVar.get<std::string>());
  std::transform(normalizedValue.begin(), normalizedValue.end(), normalizedValue.begin(),
    [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; });
  return normalizedValue == "0" || normalizedValue == "false" || normalizedValue == "no" || normalizedValue == "off";
}
Json parseEnvVars(const std::vector<std::string>& rawEnvArgs) {
  Json parsedEnv = Json::object();
  for (const auto& envStr : rawEnvArgs) {
    const auto equals = envStr.find('=');
    if (equals == 0 || equals == std::string::npos)
      throw std::runtime_error("Invalid environment variable format: " + envStr +
        ", environment variables should be added as: -e KEY1=value1 -e KEY2=value2");
    const auto key = envStr.substr(0, equals);
    // Assignment to this inherited JS setter with a string does not create a
    // property. Treat it as a prototype setter rather than an environment key.
    if (key != "__proto__") parsedEnv[key] = envStr.substr(equals + 1);
  }
  return parsedEnv;
}
} // namespace batbox
