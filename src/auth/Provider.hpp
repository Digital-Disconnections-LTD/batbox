#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::auth {
enum class ApiProvider {FirstParty,Bedrock,Vertex,Foundry};
struct ProviderKeys {
  std::string useBedrock="BATBOX_USE_BEDROCK";
  std::string useVertex="BATBOX_USE_VERTEX";
  std::string useFoundry="BATBOX_USE_FOUNDRY";
  std::string baseUrl="BATBOX_BASE_URL";
};
struct ProviderServices {
  std::function<Json(const std::string&)> environment;
  std::function<bool()> internalUser;
  std::string primaryHost,stagingHost;
  ProviderKeys keys;
};
ApiProvider getAPIProvider(const ProviderServices& services);
bool isFirstPartyBaseUrl(const ProviderServices& services);
std::string providerName(ApiProvider provider);
}
