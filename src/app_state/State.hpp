#pragma once
#include "app_state/Store.hpp"
#include "instructions/hooks/runtime/FlightHookStore.hpp"
#include "runtime/OrderedStringSet.hpp"
#include "runtime/SharedRecord.hpp"
#include <optional>

namespace batbox::app_state {
template<class Value>class OrderedStringMap {
public:
  using Entry=std::pair<std::string,Value>;
  const std::vector<Entry>& entries()const{return entries_;}
  const Value* get(const std::string& key)const{
    for(const auto& [name,value]:entries_)if(name==key)return &value;
    return nullptr;
  }
  void set(std::string key,Value value){
    for(auto& [name,stored]:entries_)if(name==key){stored=std::move(value);return;}
    entries_.emplace_back(std::move(key),std::move(value));
  }
  bool erase(const std::string& key){
    for(auto it=entries_.begin();it!=entries_.end();++it)
      if(it->first==key){entries_.erase(it);return true;}
    return false;
  }
  std::size_t size()const{return entries_.size();}
private:
  std::vector<Entry> entries_;
};

struct AppState {
  value::RecordRef fields;
  std::shared_ptr<OrderedStringMap<value::ValueRef>> missions;
  std::shared_ptr<OrderedStringMap<std::string>> batNameRoster;
  std::shared_ptr<instructions::hooks::runtime::FlightHooksState> flightHooks;
  std::shared_ptr<runtime::OrderedStringSet> trackedFiles,activeOverlays;
  value::RecordRef permissionContext;
  value::ValueRef settingsEnv;
  Json snapshot()const{return fields->snapshot();}
  std::vector<std::string> ownKeys()const{return fields->keys();}
  AppState shallowCopy()const{
    AppState copy=*this;
    copy.fields=std::make_shared<value::SharedRecord>(fields->shallowCopy());
    return copy;
  }
  value::ValueRef fieldRef(const std::string& key)const{return fields->get(key);}
  void setSettings(value::ValueRef settings,value::ValueRef env){
    fields->setRef("settings",std::move(settings));
    settingsEnv=std::move(env);
  }
  void setPermissionContext(value::RecordRef context){
    permissionContext=std::move(context);
    fields->set("toolPermissionContext",permissionContext->snapshot());
  }
};
using AppStateRef=std::shared_ptr<AppState>;
using AppStateStore=Store<AppStateRef>;

struct DefaultAppStateServices {
  std::function<bool()> isPartner,isPlanModeRequired;
  std::function<Json()> getInitialSettings,getEmptyGadgetPermissionContext,
    createEmptyAttributionState,shouldEnableThinkingByDefault,
    shouldEnablePoetrySuggestion;
  value::ValueRef idleSpeculationState;
};
AppStateRef getDefaultAppState(const DefaultAppStateServices& services);
} // namespace batbox::app_state
