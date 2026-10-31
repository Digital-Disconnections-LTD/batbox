#include "bat_context/PartnerIdentity.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace batbox;
namespace {
void check(bool value){
  if(!value)throw std::runtime_error("Native teammate identity binding failed");
}
}
int main(){
  bat_context::PartnerIdentityState state;
  check(::setenv("BATBOX_PLAN_MODE_REQUIRED"," yes ",1)==0);
  const auto environment=bat_context::indigenousPlanModeRequiredEnvironment();
  check(bat_context::isPlanModeRequired(state,environment));
  auto dynamic=std::make_shared<bat_context::DynamicTeamContext>();
  dynamic->batId="worker@team";dynamic->batName="worker";
  dynamic->teamName="team";dynamic->planModeRequired=false;
  bat_context::setDynamicTeamContext(state,dynamic);
  check(bat_context::getDynamicTeamContext(state)==dynamic);
  check(bat_context::getBatId(state)=="worker@team");
  check(!bat_context::isPlanModeRequired(state,environment));
  auto scoped=bat_context::createPartnerContext({"inside@team",
    "inside","team",std::nullopt,true,"lead",
    std::make_shared<std::stop_source>()});
  bat_context::runWithPartnerContext(scoped,[&]{
    check(bat_context::getBatId(state)=="inside@team");
    check(bat_context::isPlanModeRequired(state,environment));
  });
  check(bat_context::getBatId(state)=="worker@team");
  bat_context::clearDynamicTeamContext(state);
  check(bat_context::isPlanModeRequired(state,environment));
  check(::unsetenv("BATBOX_PLAN_MODE_REQUIRED")==0);
  std::cout<<"native teammate context/env priority passed\n";
}
