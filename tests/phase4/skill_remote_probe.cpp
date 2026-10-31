#include "skills/remote/RemoteSkill.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

using batbox::Json;
namespace remote=batbox::skills::remote;
namespace value=batbox::value;
namespace {
std::string value(const Json& object,const char* key){
  const auto& field=value::property(object,key);
  return field.is_string()?field.get<std::string>():std::string{};
}
Json run(const Json& input){
  Json events=Json::array();int serial=0;
  remote::Services services;
  services.enabled=[]{return true;};
  services.getDiscoveredRemoteSkill=[&](const std::string& slug)
      ->std::optional<remote::DiscoveredSkill>{
    events.push_back(Json::array({"discovered",slug}));
    const auto& meta=value::property(input,"meta");
    return meta.is_null()?std::nullopt:
      std::optional<remote::DiscoveredSkill>(remote::DiscoveredSkill{value(meta,"url")});
  };
  services.loadRemoteSkill=[&](const std::string& slug,
      const std::string& url,std::stop_token){
    events.push_back(Json::array({"load",slug,url}));
    const auto& fail=value::property(input,"loadError");
    if(fail.is_string())throw std::runtime_error(fail.get<std::string>());
    const auto& raw=value::property(input,"loadResult");
    return remote::LoadedSkill{value(raw,"skillPath"),value(raw,"content"),
      value::property(raw,"cacheHit")==true,
      value::numberOr(value::property(raw,"latencyMs"))};
  };
  services.recordSkillUsage=[&](const std::string& name){
    events.push_back(Json::array({"usage",name}));
  };
  services.logDebug=[&](const std::string& message){
    events.push_back(Json::array({"debug",message}));
  };
  services.getFlightId=[&]{events.push_back("session");return "session-1";};
  services.currentBatId=[&]()->std::optional<std::string>{
    const auto& id=value::property(input,"agentId");
    return id.is_string()?std::optional<std::string>(id.get<std::string>())
      :std::nullopt;
  };
  services.addInvokedSkill=[&](const std::string& name,
      const std::string& path,const std::string& content,
      const std::optional<std::string>& batId){
    events.push_back(Json::array({"invoked",name,path,content,
      batId?Json(*batId):Json(nullptr)}));
  };
  services.messageServices.randomUUID=[&]{return "id-"+std::to_string(++serial);};
  services.messageServices.nowISO=[]{return "2026-01-01T00:00:00.000Z";};
  try{
    const auto result=remote::executeRemoteSkill(value(input,"slug"),
      value(input,"commandName"),value::property(input,"parent"),services);
    Json newMessages=Json::array();
    if(result.newMessages)for(const auto& message:*result.newMessages)
      newMessages.push_back(message);
    return {{"result",Json{{"data",result.data},{"newMessages",newMessages}}},
      {"events",events}};
  }catch(const std::exception& error){
    return {{"error",error.what()},{"events",events}};
  }
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
  }
}
