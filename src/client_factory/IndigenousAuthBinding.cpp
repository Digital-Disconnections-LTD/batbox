#include "client_factory/IndigenousAuthBinding.hpp"
#include <stdexcept>

namespace batbox::client_factory {
void bindIndigenousAuth(Services& client,IndigenousAuthSources sources){
  if(!sources.loop||!sources.refresh||!sources.apiKeyHelper||
      !sources.oauthTokens||!sources.authEnabled)
    throw std::invalid_argument("Missing Indigenous client authentication provider");
  client.checkAndRefreshAuth=[loop=sources.loop,
      refresh=sources.refresh]{
    event_loop::Deferred<event_loop::Unit> output(*loop);
    refresh->check().then([output](const bool*,
        const std::exception_ptr& error){
      if(error)output.reject(error);else output.resolve({});
    });
    return output;
  };
  client.apiKeyHelper=[helper=sources.apiKeyHelper](bool nonInteractive){
    return helper->get(nonInteractive);
  };
  client.getApiKey=[apiKey=sources.apiKey]()->
      std::optional<std::string>{
    const auto result=auth::getBatboxApiKey(apiKey);
    if(result.is_null())return std::nullopt;
    if(!result.is_string())throw std::runtime_error(
      "API key source returned a non-string value");
    return result.get<std::string>();
  };
  client.getOAuthTokens=[tokens=sources.oauthTokens]{
    return tokens->getBatboxOAuthTokens();
  };
  client.isSubscriber=[tokens=sources.oauthTokens,
      enabled=sources.authEnabled,scope=sources.inferenceScope]{
    return isBatboxSubscriber(enabled,[tokens]{
      return tokens->getBatboxOAuthTokens();},scope);
  };
}
}
