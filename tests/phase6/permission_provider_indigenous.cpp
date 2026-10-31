#include "application_permissions/PermissionProvider.hpp"
#include <cassert>
#include <chrono>
#include <iostream>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  application_permissions::PermissionProvider::Inputs inputs(loop);
  inputs.interactive=false;
  auto permission=application_permissions::PermissionProvider::create(std::move(inputs));
  const Json search={{"query","Batbox fixture"}};
  const auto decide=[&](const Json& context,bool forced=false){
    auto pending=permission->check("WebSearch",search,context,forced);
    assert(pending.wait_for(std::chrono::milliseconds(0))==std::future_status::ready);
    return pending.get();
  };
  assert(decide(Json{{"mode","bypassPermissions"}}).at("behavior")=="allow");
  const auto blocked=decide(Json{{"mode","default"}});
  assert(blocked.at("behavior")=="deny");
  assert(blocked.at("message").get<std::string>().find("headless")!=std::string::npos);
  assert(decide(Json{{"mode","dontAsk"}}).at("behavior")=="deny");
  assert(decide(Json{{"mode","default"},{"alwaysAllowRules",Json::array({"WebSearch"})}})
    .at("behavior")=="allow");
  assert(decide(Json{{"mode","bypassPermissions"},{"alwaysDenyRules",Json::array({"WebSearch"})}})
    .at("behavior")=="deny");
  assert(decide(Json{{"mode","bypassPermissions"}},true).at("behavior")=="allow");
  assert(decide(Json{{"mode","default"}},true).at("behavior")=="deny");
  loop.runReady();
  std::cout<<"Headless permission decisions settle without an approval dialog\n";
}
