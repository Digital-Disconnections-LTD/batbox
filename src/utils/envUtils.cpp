#include "utils/envUtils.hpp"
#include <algorithm>
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
} // namespace batbox
