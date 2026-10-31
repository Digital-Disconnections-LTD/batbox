#include "auth/OAuthScopes.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::auth {
bool shouldUseBatboxAuth(
    const std::optional<std::vector<std::string>>& scopes,
    const std::string& inferenceScope){
  return scopes&&std::find(scopes->begin(),scopes->end(),inferenceScope)
    !=scopes->end();
}
std::vector<std::string> parseScopes(
    const std::optional<std::string>& scopeString){
  if(!scopeString)return {};
  std::vector<std::string> result;
  std::size_t from=0;
  for(;;){
    const auto next=scopeString->find(' ',from);
    const auto length=next==std::string::npos
      ?scopeString->size()-from:next-from;
    if(length>0)result.push_back(scopeString->substr(from,length));
    if(next==std::string::npos)break;
    from=next+1;
  }
  return result;
}
bool isOAuthTokenExpired(const std::optional<double>& expiresAt,
    const std::function<double()>& nowMs){
  if(!expiresAt)return false;
  if(!nowMs)throw std::invalid_argument("Missing OAuth clock binding");
  const double bufferTime=5*60*1000;
  const double now=nowMs();
  const double expiresWithBuffer=now+bufferTime;
  return expiresWithBuffer>=*expiresAt;
}
}
