#include "requests/PoetryStash.hpp"
#include <iostream>
#include <string>

using batbox::Json;
namespace req=batbox::requests;

static Json evaluate(const Json& t){
  const std::string op=t.at("op").get<std::string>();
  if(op=="split"){
    const Json& constants=t.at("promptConstants");
    req::PoetrySplitConfig config;
    config.useGlobalStashScope=t.value("global",false);
    config.skipGlobalStashForSystemPoetry=t.value("skip",false);
    config.dynamicBoundary=constants.at("dynamicBoundary").get<std::string>();
    config.attributionPrefix=constants.at("attributionPrefix").get<std::string>();
    config.systemPoetryPrefixes={"BATBOX_PROMPT_A","BATBOX_PROMPT_B"};
    return req::splitSystemPoetryPrefix(t.at("systemPrompt"),config);
  }
  if(op=="ttl"){
    req::StashTTLState state;
    if(t.contains("initialEligible")&&!t["initialEligible"].is_null()) state.userEligible=t["initialEligible"].get<bool>();
    if(t.contains("initialAllowlist")&&!t["initialAllowlist"].is_null()) state.allowlist=t["initialAllowlist"];
    req::StashTTLConfig config;
    config.provider=t.value("provider","");
    if(t.contains("bedrockOptIn")&&t["bedrockOptIn"].is_string()) config.enableOneHourForBedrock=t["bedrockOptIn"].get<std::string>();
    config.internalUser=t.value("internalUser",false);
    config.subscriber=t.value("subscriber",false);
    config.usingOverage=t.value("usingOverage",false);
    config.loadAllowlistConfig=[&t](){return t.value("allowlistConfig",Json::object());};
    Json results=Json::array();
    for(const auto& item:t.at("sources")){
      std::optional<std::string> source;
      if(item.is_string()) source=item.get<std::string>();
      results.push_back(req::shouldUseOneHourStash(source,state,config));
    }
    return Json{{"results",results},{"eligible",state.userEligible?Json(*state.userEligible):Json()},
                {"allowlist",state.allowlist?*state.allowlist:Json()}};
  }
  throw std::runtime_error("Unknown op");
}

int main(){std::string line;while(std::getline(std::cin,line)){
  try{std::cout<<Json{{"result",evaluate(Json::parse(line))}}.dump()<<'\n';}
  catch(const std::exception& error){std::cout<<Json{{"error",error.what()}}.dump()<<'\n';}
}}
