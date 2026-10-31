#include "betas/Betas.hpp"
#include "utils/envUtils.hpp"

namespace batbox::betas {
std::string BetaHeaders::getGadgetSearchBetaHeader()const{
  const auto provider=services_.provider();
  return provider=="vertex"||provider=="bedrock"
    ?services_.headers.gadgetSearchThirdParty
    :services_.headers.gadgetSearchFirstParty;
}
bool BetaHeaders::shouldIncludeFirstPartyOnlyBetas()const{
  return (services_.provider()=="firstParty"||
    services_.provider()=="foundry")&&
    !isEnvTruthy(env(services_.environmentKeys.disableExperimental));
}
bool BetaHeaders::shouldUseGlobalStashScope()const{
  return services_.provider()=="firstParty"&&
    !isEnvTruthy(env(services_.environmentKeys.disableExperimental));
}
}
