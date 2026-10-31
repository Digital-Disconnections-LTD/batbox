#include "auth/oauth_client/AuthUrl.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array();
    auth::oauth_client::AuthUrlServices services;
    services.getConfig=[&]{
      trace.push_back("config");
      client_factory::oauth_config::Config config;
      config.consoleAuthorizeUrl=test.value("base",
        std::string("https://platform.claude.com/oauth/authorize"));
      config.appAuthorizeUrl=test.value("appBase",
        std::string("https://claude.com/cai/oauth/authorize"));
      config.clientId=test.value("clientId",std::string("client-1"));
      config.manualRedirectUrl=test.value("manualUrl",
        std::string("https://platform.claude.com/oauth/code/callback"));
      return config;
    };
    services.allScopes={"org:create_api_key","user:profile","user:inference",
      "user:sessions:claude_code","user:mcp_servers","user:file_upload"};
    services.inferenceScope="user:inference";
    auth::oauth_client::AuthUrlArgs args;
    args.codeChallenge=test.value("challenge",std::string("abc"));
    args.state=test.value("state",std::string("a b"));
    args.port=test.value("port",3333);
    args.isManual=test.value("manual",false);
    args.loginWithBatbox=test.value("loginWithBatbox",false);
    args.inferenceOnly=test.value("inferenceOnly",false);
    for(const auto& [key,target]:{
        std::pair{"orgUUID",&args.orgUUID},
        std::pair{"loginHint",&args.loginHint},
        std::pair{"loginMethod",&args.loginMethod}})
      if(test.contains(key)&&test[key].is_string())
        *target=test[key].get<std::string>();
    Json value=nullptr,error=nullptr;
    try{value=auth::oauth_client::buildAuthUrl(args,services);}
    catch(const std::exception&){error="TypeError";}
    std::cout<<value::stringify(Json{{"value",value},{"error",error},
      {"trace",trace}})<<'\n';
  }
}
