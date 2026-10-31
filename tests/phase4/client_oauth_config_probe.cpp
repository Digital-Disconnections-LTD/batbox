#include "client_factory/oauth_config/WireValues.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

namespace {
batbox::Json configValue(const batbox::client_factory::oauth_config::Config& c){
  return {{"baseApiUrl",c.baseApiUrl},
    {"consoleAuthorizeUrl",c.consoleAuthorizeUrl},
    {"appAuthorizeUrl",c.appAuthorizeUrl},{"appOrigin",c.appOrigin},
    {"tokenUrl",c.tokenUrl},{"apiKeyUrl",c.apiKeyUrl},
    {"rolesUrl",c.rolesUrl},{"consoleSuccessUrl",c.consoleSuccessUrl},
    {"appSuccessUrl",c.appSuccessUrl},
    {"manualRedirectUrl",c.manualRedirectUrl},
    {"clientId",c.clientId},{"oauthFileSuffix",c.oauthFileSuffix},
    {"mcpProxyUrl",c.mcpProxyUrl},{"mcpProxyPath",c.mcpProxyPath}};
}
}
int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array();
    trace.push_back(Json::array({"env","USER_TYPE"}));
    client_factory::oauth_config::Services services;
    services.data=client_factory::oauth_config::
      makeArchivedProviderOAuthData(test.value("moduleInternal",false));
    services.environment=[&](const std::string& key)->Json{
      trace.push_back(Json::array({"env",key}));
      const auto& k=services.keys;
      const char* name=nullptr;
      if(key==k.userType)return test.value("internal",false)
        ?Json("batbox"):Json("external");
      if(key==k.useLocal)name="local";
      else if(key==k.useStaging)name="staging";
      else if(key==k.localApiBase)name="localApi";
      else if(key==k.localAppBase)name="localApp";
      else if(key==k.localConsoleBase)name="localConsole";
      else if(key==k.customBase)name="custom";
      else if(key==k.clientId)name="clientId";
      return name&&test.contains(name)?test[name]:Json(nullptr);
    };
    const auto type=client_factory::oauth_config::getOauthConfigType(
      services);
    std::string kind;
    switch(type){
      case client_factory::oauth_config::Kind::Production:
        kind="prod";break;
      case client_factory::oauth_config::Kind::Staging:
        kind="staging";break;
      case client_factory::oauth_config::Kind::Local:
        kind="local";break;
    }
    const auto suffix=client_factory::oauth_config::
      fileSuffixForOauthConfig(services);
    Json config=nullptr,error=nullptr;
    try{config=configValue(client_factory::oauth_config::
      getOauthConfig(services));}
    catch(const std::exception& caught){error=caught.what();}
    std::cout<<value::stringify(Json{{"kind",kind},{"suffix",suffix},
      {"config",config},{"error",error},{"trace",trace}})<<'\n';
  }
}
