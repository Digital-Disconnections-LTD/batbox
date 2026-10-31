#pragma once
#include "auth/ApiKey.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::auth {
struct LegacyKeyServices {
  std::function<bool()> bareMode;
  std::function<std::string()> platform;
  std::function<std::optional<Json>()> legacyKeychainPrefetch;
  std::function<std::string()> keychainServiceName;
  std::function<std::optional<std::string>(const std::string&)> executeSync;
  std::function<void(const std::string&)> logError;
  std::function<Json()> globalConfig;
};
class LegacyKeyReader {
public:
  explicit LegacyKeyReader(LegacyKeyServices services);
  std::optional<ApiKeyResult> getLoginManagedKey();
  void clearStash();
private:
  LegacyKeyServices services_;
  std::optional<std::optional<ApiKeyResult>> stashed_;
};
}
