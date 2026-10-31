#include "app_state/provider/Effects.hpp"
#include "app_state/provider/TypedSettings.hpp"
#include <iostream>

using namespace batbox;
namespace ap=batbox::app_state;
namespace provider=batbox::app_state::provider;

namespace {
ap::AppStateRef state(value::RecordRef context,value::ValueRef settings,
    value::ValueRef env){
  auto result=std::make_shared<ap::AppState>();
  result->fields=std::make_shared<value::SharedRecord>();
  result->setPermissionContext(std::move(context));
  result->setSettings(std::move(settings),std::move(env));
  result->fields->set("effortValue","low");
  result->fields->set("other",{{"stay",true}});
  return result;
}
value::RecordRef context(bool available,std::string mode="default"){
  auto value=std::make_shared<value::SharedRecord>();
  value->set("mode",std::move(mode));
  value->set("isBypassPermissionsModeAvailable",available);
  value->set("rules",{{"keep",true}});
  return value;
}
ap::AppStateChangeServices changeServices(Json& trace){
  ap::AppStateChangeServices services;
  services.isPrivilegedUserType=[]{return false;};
  services.toExternalPermissionMode=[](const std::string& mode){return mode;};
  services.notifyFlightMetadataChanged=[](value::RecordRef){};
  services.notifyPermissionModeChanged=[](const std::string&){};
  services.clearApiKeyHelperStash=[&]{trace.push_back("clearApi");};
  services.clearAwsCredentialsStash=[&]{trace.push_back("clearAws");};
  services.clearGcpCredentialsStash=[&]{trace.push_back("clearGcp");};
  services.applyConfigEnvironmentVariables=[&]{trace.push_back("applyEnv");};
  services.toError=[](std::exception_ptr error){return error;};
  services.logError=[](std::exception_ptr){};
  return services;
}
Json policyCase(bool gate,bool setting,const std::string& mode){
  Json trace=Json::array();
  auto old=context(true,mode);
  provider::BypassPolicyServices services;
  services.stashedDisableGate=[&]{trace.push_back("gate");return gate;};
  services.getSettings=[&]{trace.push_back("readSettings");
    return setting?Json{{"permissions",{{"disableBypassPermissionsMode","disable"}}}}:
      Json::object();};
  services.applyPermissionUpdate=[&](value::RecordRef value,const Json& update){
    trace.push_back("permissionUpdate");
    auto next=std::make_shared<value::SharedRecord>(value->shallowCopy());
    next->set("mode",update.at("mode"));return next;};
  const bool disabled=provider::isBypassPermissionsModeDisabled(services);
  const auto next=provider::createDisabledBypassPermissionsContext(old,services);
  const auto before=old->get("rules"),after=next->get("rules");
  return Json{{"kind","policy"},{"disabled",disabled},{"trace",trace},
    {"mode",*next->get("mode")},
    {"available",*next->get("isBypassPermissionsModeAvailable")},
    {"rootNew",next!=old},{"rulesSame",before==after}};
}
Json mountCase(bool available,bool disabled){
  event_loop::Loop loop;
  Json trace=Json::array();
  const auto oldContext=context(available);
  const auto settings=std::make_shared<Json>(Json::object());
  const auto initial=state(oldContext,settings,{});
  const auto oldOther=initial->fieldRef("other");
  auto host=std::make_shared<ap::Host>(loop,initial,changeServices(trace));
  auto listener=std::make_shared<ap::AppStateStore::Listener>([&]{
    trace.push_back("listener");});
  auto unsubscribe=host->subscribe(listener);
  provider::MountServices services;
  services.policy.stashedDisableGate=[&]{trace.push_back("gate");return disabled;};
  services.policy.getSettings=[&]{trace.push_back("readSettings");return Json::object();};
  services.policy.applyPermissionUpdate=[](value::RecordRef,const Json&)
    ->value::RecordRef{throw std::logic_error("Unreachable mode update");};
  services.debug=[&](const std::string&){trace.push_back("debug");};
  provider::runMountPermissionCheck(*host,services);
  const auto next=host->getState();
  unsubscribe();
  return Json{{"kind","mount"},{"trace",trace},{"rootNew",next!=initial},
    {"contextNew",next->permissionContext!=oldContext},
    {"otherSame",next->fieldRef("other")==oldOther},
    {"available",*next->permissionContext->get(
      "isBypassPermissionsModeAvailable")}};
}
Json settingsCase(bool sameSettings,bool effort){
  event_loop::Loop loop;
  Json trace=Json::array();
  const auto env=std::make_shared<Json>(Json{{"keep",true}});
  const auto oldSettings=std::make_shared<Json>(Json{{"env",*env}});
  const auto newSettings=sameSettings?oldSettings:
    std::make_shared<Json>(Json{{"env",*env},{"effortLevel",effort?"high":"low"}});
  const auto oldContext=context(false);
  const auto initial=state(oldContext,oldSettings,env);
  const auto oldOther=initial->fieldRef("other");
  auto host=std::make_shared<ap::Host>(loop,initial,changeServices(trace));
  auto listener=std::make_shared<ap::AppStateStore::Listener>([&]{
    trace.push_back("listener");});
  auto unsubscribe=host->subscribe(listener);
  provider::TypedSettingsServices services;
  services.getInitialSettings=[&]{trace.push_back("initialSettings");
    return provider::SettingsRecord{newSettings,env};};
  services.debug=[&](const std::string&){trace.push_back("debug");};
  services.loadAllPermissionRulesFromDisk=[&]{trace.push_back("rules");
    return Json::array();};
  services.updateHooksConfigSnapshot=[&]{trace.push_back("hooks");};
  services.syncPermissionRulesFromDisk=[&](value::RecordRef value,const Json&){
    trace.push_back("sync");
    return std::make_shared<value::SharedRecord>(value->shallowCopy());};
  services.transitionPlanAutoMode=[&](value::RecordRef value){
    trace.push_back("transition");return value;};
  provider::applySettingsChangeToHost(settings::Source::User,*host,services);
  const auto next=host->getState();
  unsubscribe();
  return Json{{"kind","settings"},{"trace",trace},
    {"rootNew",next!=initial},{"contextNew",next->permissionContext!=oldContext},
    {"settingsSame",next->fieldRef("settings")==oldSettings},
    {"otherSame",next->fieldRef("other")==oldOther},
    {"effortValue",*next->fieldRef("effortValue")}};
}
Json listenerCase(){
  event_loop::Loop loop;
  Json trace=Json::array();
  settings::persistence::InternalWrites writes([]{return 0.0;});
  settings::persistence::ChangeEventServices eventServices;
  eventServices.internalWrites=&writes;
  eventServices.debug=[](const std::string&){};
  eventServices.executeConfigChangeHooks=[](const std::string&,
      const std::string&,std::function<void(const std::vector<bool>&)> done){
    done({});};
  eventServices.setTimeout=[](double,std::function<void()>){
    return []{};};
  eventServices.resetSettingsStash=[&]{trace.push_back("reset");};
  settings::persistence::ChangeEvents detector(eventServices);
  auto oldContext=context(false);
  auto oldSettings=std::make_shared<Json>(Json::object());
  auto host=std::make_shared<ap::Host>(loop,
    state(oldContext,oldSettings,{}),changeServices(trace));
  provider::SettingsListenerServices listenerServices;
  listenerServices.getSettingsDeprecated=[&]{
    trace.push_back("readSettings");return Json::object();};
  listenerServices.applySettingsChange=[&](settings::Source,ap::Host&){
    trace.push_back("apply");};
  provider::SettingsSubscription subscription(loop,detector,host,
    listenerServices);
  detector.notifyChange(settings::Source::User);
  subscription.close();
  detector.notifyChange(settings::Source::User);
  return Json{{"kind","listener"},{"trace",trace}};
}
}
int main(){
  Json rows=Json::array();
  rows.push_back(policyCase(false,false,"default"));
  rows.push_back(policyCase(true,false,"bypassPermissions"));
  rows.push_back(policyCase(false,true,"bypassPermissions"));
  rows.push_back(mountCase(false,true));
  rows.push_back(mountCase(true,false));
  rows.push_back(mountCase(true,true));
  rows.push_back(settingsCase(true,false));
  rows.push_back(settingsCase(false,true));
  rows.push_back(listenerCase());
  std::cout<<rows.dump()<<'\n';
}
