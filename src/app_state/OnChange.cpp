#include "app_state/OnChange.hpp"
#include <stdexcept>

namespace batbox::app_state {
namespace {
void need(bool present,const char* name){
  if(!present)throw std::invalid_argument(name);
}
const Json& field(const AppStateRef& state,const char* key){
  const auto value=state->fieldRef(key);
  if(value)return *value;
  return value::property(Json(nullptr),key);
}
bool differs(const AppStateRef& a,const AppStateRef& b,const char* key){
  const auto left=a->fieldRef(key),right=b->fieldRef(key);
  if(!left||!right)return bool(left)!=bool(right);
  return *left!=*right;
}
bool configDiffers(const Json& config,const char* key,const Json& value){
  return !config.is_object()||!config.contains(key)||config.at(key)!=value;
}
std::string mode(const AppStateRef& state){
  if(!state->permissionContext)
    throw std::invalid_argument("Permission context record required");
  const auto value=state->permissionContext->get("mode");
  if(!value||!value->is_string())
    throw std::invalid_argument("Permission mode value required");
  return value->get<std::string>();
}
void save(const AppStateChangeServices& services,
  std::function<Json(const Json&)> updater){
  need(bool(services.saveGlobalConfig),"Global config saver required");
  services.saveGlobalConfig(updater);
}
}
void onChangeAppState(const AppStateRef& next,const AppStateRef& old,
  const AppStateChangeServices& services){
  const auto prevMode=mode(old);
  const auto newMode=mode(next);
  if(prevMode!=newMode){
    need(bool(services.toExternalPermissionMode),
      "External permission mode import required");
    const auto prevExternal=services.toExternalPermissionMode(prevMode);
    const auto newExternal=services.toExternalPermissionMode(newMode);
    if(prevExternal!=newExternal){
      need(bool(services.notifyFlightMetadataChanged),
        "Session metadata notifier required");
      const bool ultramode=newExternal=="plan"&&
        value::truthy(field(next,"isUltraplanMode"))&&
        !value::truthy(field(old,"isUltraplanMode"));
      auto metadata=std::make_shared<value::SharedRecord>();
      metadata->set("permission_mode",newExternal);
      metadata->set("is_ultraplan_mode",ultramode?Json(true):Json(nullptr));
      services.notifyFlightMetadataChanged(std::move(metadata));
    }
    need(bool(services.notifyPermissionModeChanged),
      "Permission mode notifier required");
    services.notifyPermissionModeChanged(newMode);
  }

  if(differs(next,old,"mainLoopModel")&&
    field(next,"mainLoopModel").is_null()){
    need(bool(services.updateSettingsForSource),
      "Settings source updater required");
    auto patch=std::make_shared<value::SharedRecord>();
    patch->setRef("model",{});
    services.updateSettingsForSource("userSettings",std::move(patch));
    need(bool(services.setMainLoopModelOverride),
      "Main model override import required");
    services.setMainLoopModelOverride(nullptr);
  }
  if(differs(next,old,"mainLoopModel")&&
    !field(next,"mainLoopModel").is_null()){
    need(bool(services.updateSettingsForSource),
      "Settings source updater required");
    auto patch=std::make_shared<value::SharedRecord>();
    patch->set("model",field(next,"mainLoopModel"));
    services.updateSettingsForSource("userSettings",std::move(patch));
    need(bool(services.setMainLoopModelOverride),
      "Main model override import required");
    services.setMainLoopModelOverride(field(next,"mainLoopModel"));
  }

  if(differs(next,old,"expandedView")){
    const bool todos=field(next,"expandedView")=="tasks";
    const bool tree=field(next,"expandedView")=="teammates";
    need(bool(services.getGlobalConfig),"Global config getter required");
    if(configDiffers(services.getGlobalConfig(),"showExpandedTodos",todos)||
      configDiffers(services.getGlobalConfig(),"showSpinnerTree",tree)){
      save(services,[todos,tree](const Json& current){
        Json updated=current;
        updated["showExpandedTodos"]=todos;
        updated["showSpinnerTree"]=tree;
        return updated;
      });
    }
  }
  if(differs(next,old,"verbose")){
    need(bool(services.getGlobalConfig),"Global config getter required");
    if(configDiffers(services.getGlobalConfig(),"verbose",field(next,"verbose"))){
      const Json verbose=field(next,"verbose");
      save(services,[verbose](const Json& current){
        Json updated=current;updated["verbose"]=verbose;return updated;
      });
    }
  }
  need(bool(services.isPrivilegedUserType),
    "Privileged user-type import required");
  if(services.isPrivilegedUserType()&&
    differs(next,old,"tungstenPanelVisible")&&
    next->fieldRef("tungstenPanelVisible")){
    need(bool(services.getGlobalConfig),"Global config getter required");
    const auto visible=field(next,"tungstenPanelVisible");
    if(configDiffers(services.getGlobalConfig(),
        "tungstenPanelVisible",visible)){
      save(services,[visible](const Json& current){
        Json updated=current;
        updated["tungstenPanelVisible"]=visible;
        return updated;
      });
    }
  }

  if(next->fieldRef("settings")!=old->fieldRef("settings")){
    need(bool(services.clearApiKeyHelperStash),
      "API-key helper cache import required");
    need(bool(services.clearAwsCredentialsStash),
      "Cloud credential cache import required");
    need(bool(services.clearGcpCredentialsStash),
      "Second cloud credential cache import required");
    need(bool(services.applyConfigEnvironmentVariables),
      "Managed environment import required");
    need(bool(services.toError),"Local error converter required");
    need(bool(services.logError),"Local error logger required");
    try{
      services.clearApiKeyHelperStash();
      services.clearAwsCredentialsStash();
      services.clearGcpCredentialsStash();
      if(next->settingsEnv!=old->settingsEnv)
        services.applyConfigEnvironmentVariables();
    }catch(...){
      services.logError(services.toError(std::current_exception()));
    }
  }
}
} // namespace batbox::app_state
