#include "client_factory/IndigenousAuthBinding.hpp"
#include "auth/OAuthScopes.hpp"
#include <stdexcept>
#include <vector>

namespace batbox::client_factory {
namespace {
std::optional<std::vector<std::string>> scopes(
    const std::optional<Json>& tokens){
  if(!tokens||!tokens->is_object()||!tokens->contains("scopes")||
      !(*tokens)["scopes"].is_array())return std::nullopt;
  std::vector<std::string> result;
  for(const auto& value:(*tokens)["scopes"]){
    if(!value.is_string())return std::nullopt;
    result.push_back(value.get<std::string>());
  }
  return result;
}
}
bool isBatboxSubscriber(const std::function<bool()>& authEnabled,
    const std::function<std::optional<Json>()>& oauthTokens,
    const std::string& inferenceScope){
  if(!authEnabled||!oauthTokens)
    throw std::invalid_argument("Missing subscriber auth binding");
  if(!authEnabled())return false;
  return auth::shouldUseBatboxAuth(scopes(oauthTokens()),inferenceScope);
}
}
