#include "app_state/OnChange.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace {
std::string errorText(std::exception_ptr error){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::exception& value){return value.what();}
  return "";
}
app_state::AppStateRef initial(const Json& item){
  auto state=std::make_shared<app_state::AppState>();
  state->fields=std::make_shared<value::SharedRecord>();
  state->permissionContext=std::make_shared<value::SharedRecord>();
  state->permissionContext->set("mode","default");
  state->permissionContext->set("rules",{{"x",1}});
  state->fields->set("toolPermissionContext",state->permissionContext->snapshot());
  state->fields->set("isUltraplanMode",false);
  state->fields->set("mainLoopModel",
    item.contains("oldModel")?item.at("oldModel"):Json(nullptr));
  state->fields->set("expandedView","none");
  state->fields->set("verbose",false);
  state->fields->set("tungstenPanelVisible",false);
  state->settingsEnv=std::make_shared<Json>(Json{{"A","one"}});
  state->fields->set("settings",{{"env",*state->settingsEnv}});
  return state;
}
}
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);Json cases;file>>cases;
  Json rows=Json::array();
  for(const auto& item:cases){
    Json trace=Json::array();
    Json config={{"showExpandedTodos",false},{"showSpinnerTree",false},
      {"verbose",false},{"tungstenPanelVisible",false}};
    const auto old=initial(item);
    app_state::AppStateChangeServices services;
    services.permissionModeFromString=[&](const std::string& mode){
      trace.push_back(Json::array({"parseMode",mode}));return mode;};
    services.toExternalPermissionMode=[&](const std::string& mode){
      trace.push_back(Json::array({"external",mode}));
      return mode=="bubble"?std::string("default"):mode;};
    services.notifyFlightMetadataChanged=[&](value::RecordRef metadata){
      trace.push_back(Json::array({"metadata",metadata->snapshot()}));};
    services.notifyPermissionModeChanged=[&](const std::string& mode){
      trace.push_back(Json::array({"permission",mode}));};
    services.updateSettingsForSource=[&](const std::string& source,
      value::RecordRef patch){
      trace.push_back(Json::array({"settingsUpdate",source,
        patch->keys(),patch->snapshot()}));};
    services.setMainLoopModelOverride=[&](const Json& model){
      trace.push_back(Json::array({"modelOverride",model}));};
    services.getGlobalConfig=[&]{
      trace.push_back(Json::array({"getConfig"}));return config;};
    services.saveGlobalConfig=[&](const std::function<Json(const Json&)>& update){
      config=update(config);
      trace.push_back(Json::array({"saveConfig",config}));};
    services.isPrivilegedUserType=[&]{
      trace.push_back(Json::array({"userType"}));
      return item.value("privileged",false);};
    services.clearApiKeyHelperStash=[&]{
      trace.push_back(Json::array({"clearApi"}));
      if(item.value("throwCache",false))
        throw std::runtime_error("cache failed");};
    services.clearAwsCredentialsStash=[&]{
      trace.push_back(Json::array({"clearAws"}));};
    services.clearGcpCredentialsStash=[&]{
      trace.push_back(Json::array({"clearGcp"}));};
    services.applyConfigEnvironmentVariables=[&]{
      trace.push_back(Json::array({"applyEnv"}));};
    services.toError=[&](std::exception_ptr error){
      trace.push_back(Json::array({"toError",errorText(error)}));
      return error;};
    services.logError=[&](std::exception_ptr error){
      trace.push_back(Json::array({"logError",errorText(error)}));};

    if(item.at("kind")=="metadata"){
      Json metadata=Json::object();
      if(item.contains("permission_mode"))
        metadata["permission_mode"]=item.at("permission_mode");
      if(item.contains("is_ultraplan_mode"))
        metadata["is_ultraplan_mode"]=item.at("is_ultraplan_mode");
      const auto next=app_state::externalMetadataToAppState(metadata,
        services.permissionModeFromString)(old);
      rows.push_back({{"name",item.at("name")},{"trace",trace},
        {"value",next->snapshot()},
        {"identity",{{"root",next!=old},
          {"context",next->permissionContext==old->permissionContext},
          {"rules",next->permissionContext->get("rules")==
            old->permissionContext->get("rules")},
          {"settings",next->fieldRef("settings")==old->fieldRef("settings")}}}});
    }else{
      auto next=std::make_shared<app_state::AppState>(old->shallowCopy());
      if(item.contains("mode")){
        auto context=std::make_shared<value::SharedRecord>(
          old->permissionContext->shallowCopy());
        context->set("mode",item.at("mode"));
        next->setPermissionContext(std::move(context));
      }
      for(const char* key:{"ultraplan","model","expandedView","verbose",
          "visible"})if(item.contains(key)){
        const std::string target=std::string(key)=="ultraplan"?
          "isUltraplanMode":std::string(key)=="model"?"mainLoopModel":
          std::string(key)=="visible"?"tungstenPanelVisible":key;
        next->fields->set(target,item.at(key));
      }
      if(item.value("settingsNew",false)){
        auto env=item.value("envNew",false)?
          std::make_shared<Json>(Json{{"A","two"}}):old->settingsEnv;
        next->setSettings(std::make_shared<Json>(Json{{"env",*env}}),env);
      }
      app_state::onChangeAppState(next,old,services);
      rows.push_back({{"name",item.at("name")},{"trace",trace}});
    }
  }
  std::cout<<rows.dump()<<'\n';
}
