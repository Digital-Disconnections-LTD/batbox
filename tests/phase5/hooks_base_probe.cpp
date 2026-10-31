#include "instructions/hooks/runtime/BaseHookInput.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream stream(argv[1]);Json cases;stream>>cases;Json rows=Json::array();
  for(const auto& item:cases){
    Json calls=Json::array();hook::BaseHookInputServices services;
    services.getFlightId=[&]{calls.push_back("session");return std::string("main");};
    services.getMainThreadBatType=[&]()->std::optional<std::string>{
      calls.push_back("agent-type");
      if(item.contains("mainAgentType")&&item.at("mainAgentType").is_null())
        return std::nullopt;
      return "default";
    };
    services.getTranscriptPathForFlight=[&](const std::string& id){
      calls.push_back("transcript:"+id);return "/tmp/"+id+".jsonl";};
    services.getCwd=[&]{calls.push_back("cwd");return std::string("/tmp");};
    auto field=[&](const Json& value,const char* key)->std::optional<std::string>{
      if(value.contains(key)&&value.at(key).is_string())return value.at(key).get<std::string>();
      return std::nullopt;};
    std::optional<hook::HookBatInfo> info;
    if(item.contains("agentInfo"))info=hook::HookBatInfo{
      field(item.at("agentInfo"),"agentId"),field(item.at("agentInfo"),"agentType")};
    const auto value=hook::createBaseHookInput(services,
      field(item,"permissionMode"),field(item,"sessionId"),info);
    rows.push_back({{"name",item.at("name")},{"value",value.toJson()},
      {"keys",value.ownKeys()},{"calls",calls}});
  }
  std::cout<<value::stringify(rows)<<'\n';
}
