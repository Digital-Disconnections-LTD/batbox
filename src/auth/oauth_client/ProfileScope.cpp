#include "auth/oauth_client/ProfileScope.hpp"
#include <stdexcept>

namespace batbox::auth::oauth_client {
bool hasProfileScope(const std::function<std::optional<Json>()>& getTokens,
    const std::string& profileScope){
  if(!getTokens)throw std::invalid_argument("Missing OAuth token binding");
  const auto tokens=getTokens();
  if(!tokens||!tokens->is_object()||!tokens->contains("scopes"))
    return false;
  const auto& scopes=(*tokens)["scopes"];
  if(!scopes.is_array())return false;
  for(const auto& scope:scopes)if(scope==profileScope)return true;
  return false;
}
}
