#include "bat_context/PartnerIdentity.hpp"
#include "runtime/JsonValue.hpp"
#include "utils/envUtils.hpp"
#include <iostream>
#include <memory>
#include <string>

using namespace batbox;
namespace {
std::optional<std::string> optionalString(const Json& value){
  return value.is_string()?std::optional(value.get<std::string>()):std::nullopt;
}
Json value(const std::optional<std::string>& text){
  return text?Json(*text):Json();
}
Json snapshot(bat_context::PartnerIdentityState& state,
    const Json& input){
  const auto environment=[&]{return isEnvTruthy(value::property(input,"env"));};
  return Json{{"parentSessionId",value(bat_context::getParentFlightId(state))},
    {"agentId",value(bat_context::getBatId(state))},
    {"agentName",value(bat_context::getBatName(state))},
    {"teamName",value(bat_context::getTeamName(state,
      optionalString(value::property(input,"appStateTeam"))))},
    {"isTeammate",bat_context::isPartner(state)},
    {"color",value(bat_context::getPartnerColor(state))},
    {"planModeRequired",bat_context::isPlanModeRequired(state,environment)},
    {"isTeamLead",bat_context::isTeamLead(state,
      optionalString(value::property(input,"lead")))}};
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const auto input=value::parse(line);
    bat_context::PartnerIdentityState state;
    std::shared_ptr<bat_context::DynamicTeamContext> dynamic;
    const auto& data=value::property(input,"dynamic");
    if(data.is_object()){
      dynamic=std::make_shared<bat_context::DynamicTeamContext>();
      dynamic->batId=value::property(data,"agentId").get<std::string>();
      dynamic->batName=value::property(data,"agentName").get<std::string>();
      dynamic->teamName=value::property(data,"teamName").get<std::string>();
      dynamic->color=optionalString(value::property(data,"color"));
      dynamic->planModeRequired=value::property(data,"planModeRequired")==true;
      dynamic->parentFlightId=optionalString(
        value::property(data,"parentSessionId"));
    }
    const auto& scoped=value::property(input,"scoped");
    bat_context::PartnerRef mate;
    if(scoped.is_object()){
      bat_context::PartnerSpawnConfig config;
      config.batId=value::property(scoped,"agentId").get<std::string>();
      config.batName=value::property(scoped,"agentName").get<std::string>();
      config.teamName=value::property(scoped,"teamName").get<std::string>();
      config.color=optionalString(value::property(scoped,"color"));
      config.planModeRequired=value::property(scoped,"planModeRequired")==true;
      config.parentFlightId=value::property(scoped,"parentSessionId")
        .get<std::string>();
      config.abortController=std::make_shared<std::stop_source>();
      mate=bat_context::createPartnerContext(config);
    }
    const auto output=bat_context::runWithPartnerContext(mate,[&]{
      bat_context::setDynamicTeamContext(state,dynamic);
      const auto first=snapshot(state,input);
      const bool same=bat_context::getDynamicTeamContext(state)==dynamic;
      bat_context::clearDynamicTeamContext(state);
      const bool cleared=!bat_context::getDynamicTeamContext(state);
      const auto afterClear=snapshot(state,input);
      return Json{{"first",first},{"same",same},
        {"cleared",cleared},{"afterClear",afterClear}};
    });
    std::cout<<value::stringify(output)<<'\n';
  }
}
