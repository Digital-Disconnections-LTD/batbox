#include "bats/TypedBatState.hpp"
#include <iostream>

using namespace batbox;
struct Case {
  std::string name;
  Json bat;
  bool isAsync=false;
  std::optional<bool> canShow;
  std::optional<Json> allowed;
  bool classifier=false;
  std::string mode="default";
};
int main(){
  const std::vector<Case> cases={
    {"unchanged",Json::object()},
    {"mode-effort",Json{{"permissionMode","plan"},{"effort","high"}}},
    {"async-deny",Json::object(),true},
    {"bubble-async",Json{{"permissionMode","bubble"}},true},
    {"allowed",Json{{"permissionMode","plan"}},false,{},Json::array({"Read"})},
    {"bypass",Json{{"permissionMode","plan"}},false,{},
      {},false,"bypassPermissions"},
    {"auto",Json{{"permissionMode","plan"}},false,{},
      {},true,"auto"},
    {"async-can-show",Json::object(),true,true},
    {"sync-cannot-show",Json::object(),false,false}
  };
  for(const auto& test:cases){
    auto state=std::make_shared<app_state::AppState>();
    state->fields=std::make_shared<value::SharedRecord>();
    state->fields->set("effortValue","medium");
    state->fields->set("agentDefinitions",Json{{"id","stable"}});
    state->setPermissionContext(std::make_shared<value::SharedRecord>(
      value::SharedRecord::fromJson(Json{{"mode",test.mode},
        {"alwaysAllowRules",Json{{"cliArg",Json::array({"Bash"})},
          {"session",Json::array({"old"})}}}})));
    const auto bats=state->fieldRef("agentDefinitions");
    auto get=bats::createTypedBatGetAppState([state]{return state;},
      test.bat,test.isAsync,test.canShow,test.allowed,
      [classifier=test.classifier](const auto& name){
        return name=="TRANSCRIPT_CLASSIFIER"&&classifier;});
    const auto result=get();
    const auto permission=result->permissionContext->snapshot();
    const auto& avoid=value::property(permission,"shouldAvoidPermissionPrompts");
    const auto& await=value::property(permission,"awaitAutomatedChecksBeforeDialog");
    const auto& effort=result->fieldRef("effortValue");
    std::cout<<Json{{"name",test.name},{"sameState",result==state},
      {"samePermission",result->permissionContext==state->permissionContext},
      {"mode",value::property(permission,"mode")},
      {"avoid",avoid.is_discarded()?Json():avoid},
      {"await",await.is_discarded()?Json():await},
      {"rules",value::property(permission,"alwaysAllowRules")},
      {"effort",effort?*effort:Json()},
      {"sharedAgents",result->fieldRef("agentDefinitions")==bats}}.dump()<<'\n';
  }
}
