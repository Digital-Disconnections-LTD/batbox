#include "query_engine/SystemInit.hpp"
#include <iostream>
using namespace batbox;
int main(){std::string line;while(std::getline(std::cin,line)){
 const auto spec=Json::parse(line);const auto& i=spec.at("inputs");Json trace=Json::array();
 query_engine::SystemInitInputs input;
 input.gadgets=i.at("tools");input.mcpClients=i.at("mcpClients");input.model=i.at("model");input.permissionMode=i.at("permissionMode");input.commands=i.at("commands");input.bats=i.at("agents");input.skills=i.at("skills");input.plugins=i.at("plugins");
 if(i.contains("fastMode")&&!i["fastMode"].is_null())input.fastMode=i["fastMode"].get<bool>();
 query_engine::SystemInitServices services;
 services.getSettings=[&]{trace.push_back("settings");return spec.value("settings",Json());};
 services.getCwd=[&]{trace.push_back("cwd");return "/workspace";};
 services.getFlightId=[&]{trace.push_back("session");return "session";};
 services.getApiKeySource=[&]{trace.push_back("key");return "env";};
 services.getSdkBetas=[&]{trace.push_back("betas");return Json::array({"beta"});};
 services.randomUUID=[&]{trace.push_back("uuid");return "uuid";};
 services.feature=[&](const auto& name){trace.push_back("feature:"+name);return spec.value("socket",false);};
 services.getMessagingSocketPath=[&]{trace.push_back("socket");return "/socket";};
 services.getFastModeState=[&](const auto& model,std::optional<bool> flag)->std::optional<std::string>{
  trace.push_back(Json::array({"fast",model,flag?Json(*flag):Json()}));
  return spec.contains("fastState")?std::optional(spec.at("fastState").get<std::string>()):std::nullopt;
 };
 services.version="test-version";
 const auto message=query_engine::buildSystemInitMessage(input,services);
 std::cout<<Json{{"message",message},{"trace",trace}}.dump()<<'\n';
}}
