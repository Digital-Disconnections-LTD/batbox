#include "betas/Betas.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::betas {
BetaHeaders::BetaHeaders(Services services):services_(std::move(services)){}
Json BetaHeaders::env(const std::string& key)const{
  if(!services_.environment)
    throw std::invalid_argument("Missing beta environment binding");
  return services_.environment(key);
}
bool BetaHeaders::internalCli()const{
  return env(services_.environmentKeys.userType)==
      services_.environmentKeys.internalUserType&&
    env(services_.environmentKeys.entrypoint)=="cli";
}
std::optional<std::vector<std::string>>
BetaHeaders::filterAllowedSdkBetas(
    const std::optional<std::vector<std::string>>& sdkBetas){
  if(!sdkBetas||sdkBetas->empty())return std::nullopt;
  if(services_.isSubscriber()){
    services_.warn("Warning: Custom betas are only available for API key "
      "users. Ignoring provided betas.");
    return std::nullopt;
  }
  std::vector<std::string> allowed,disallowed;
  for(const auto& beta:*sdkBetas){
    if(beta==services_.headers.context1m)allowed.push_back(beta);
    else disallowed.push_back(beta);
  }
  for(const auto& beta:disallowed)
    services_.warn("Warning: Beta header '"+beta+
      "' is not allowed. Only the following betas are supported: "+
      services_.headers.context1m);
  return allowed.empty()?std::nullopt:
    std::optional<std::vector<std::string>>(std::move(allowed));
}
}
