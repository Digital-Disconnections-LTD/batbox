#include "auth/oauth_client/AuthUrl.hpp"
#include "auth/oauth_client/FormUrl.hpp"
#include "runtime/Url.hpp"
#include <stdexcept>

namespace batbox::auth::oauth_client {
namespace {
std::string joinScopes(const std::vector<std::string>& scopes){
  std::string joined;
  for(const auto& scope:scopes){
    if(!joined.empty())joined.push_back(' ');
    joined+=scope;
  }
  return joined;
}
}
std::string buildAuthUrl(const AuthUrlArgs& args,
    const AuthUrlServices& services){
  if(!services.getConfig)
    throw std::invalid_argument("Missing OAuth configuration binding");
  const auto baseConfig=services.getConfig();
  const auto base=args.loginWithBatbox?baseConfig.appAuthorizeUrl:
    baseConfig.consoleAuthorizeUrl;
  const auto parsed=runtime::parseUrl(base);
  if(!parsed)throw std::invalid_argument("Invalid OAuth authorization URL");
  QueryEntries params;
  params.emplace_back("code","true");
  params.emplace_back("client_id",services.getConfig().clientId);
  params.emplace_back("response_type","code");
  params.emplace_back("redirect_uri",args.isManual?
    services.getConfig().manualRedirectUrl:
    "http://localhost:"+std::to_string(args.port)+"/callback");
  params.emplace_back("scope",args.inferenceOnly?services.inferenceScope:
    joinScopes(services.allScopes));
  params.emplace_back("code_challenge",args.codeChallenge);
  params.emplace_back("code_challenge_method","S256");
  params.emplace_back("state",args.state);
  if(args.orgUUID&&!args.orgUUID->empty())
    params.emplace_back("orgUUID",*args.orgUUID);
  if(args.loginHint&&!args.loginHint->empty())
    params.emplace_back("login_hint",*args.loginHint);
  if(args.loginMethod&&!args.loginMethod->empty())
    params.emplace_back("login_method",*args.loginMethod);
  return appendFormQuery((*parsed)["href"].get<std::string>(),params);
}
}
