#include "application_compaction_settings/Policy.hpp"
#include "application/Async.hpp"
#include "application_config/EndpointStore.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::application_compaction_settings {

Policy::Policy(event_loop::Loop& owner,const application::Config& config,
    std::shared_ptr<app_state::AppStateStore> store):owner_(owner),
    store_(std::move(store)),path_(config.configPath),
    fallbackPath_(config.legacyConfigDirectory.empty()?std::filesystem::path{}:
      config.legacyConfigDirectory/"native-client.json"),
    enabled_(!value::property(config.settings,"autoCompactEnabled").is_boolean()||
      value::property(config.settings,"autoCompactEnabled").get<bool>()),
    compactAt_(config.compactAt){
  if(!store_)throw std::invalid_argument("Compaction policy needs app state");
  if(config.compactAt==0)throw std::invalid_argument("Compaction threshold must be positive");
}
std::shared_ptr<Policy> Policy::create(event_loop::Loop& owner,
    const application::Config& config,
    std::shared_ptr<app_state::AppStateStore> store){
  auto result=std::shared_ptr<Policy>(new Policy(owner,config,std::move(store)));
  result->projectEnabled(result->enabled());
  result->projectThreshold(result->compactAt());
  return result;
}
void Policy::projectEnabled(bool value){
  store_->setState([value](const app_state::AppStateRef& previous){
    Json settings=value::property(previous->snapshot(),"settings");
    if(!settings.is_object())settings=Json::object();
    if(value::property(settings,"autoCompactEnabled")==value)return previous;
    settings["autoCompactEnabled"]=value;
    auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
    next->fields->set("settings",settings);return next;
  });
}
void Policy::projectThreshold(std::size_t value){
  store_->setState([value](const app_state::AppStateRef& previous){
    Json settings=value::property(previous->snapshot(),"settings");
    if(!settings.is_object())settings=Json::object();
    if(value::property(settings,"compactAt")==value)return previous;
    settings["compactAt"]=value;
    auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
    next->fields->set("settings",settings);return next;
  });
}
event_loop::Deferred<event_loop::Unit> Policy::change(const char* key,Json value){
  event_loop::Deferred<event_loop::Unit> output(owner_);
  try{
    if(!owner_.isOwnerThread())throw std::logic_error(
      "Compaction policy change requires owner loop");
    if(busy_)throw std::runtime_error("Compaction settings are already saving");
    busy_=true;
    auto self=shared_from_this();
    application::runInWorker<event_loop::Unit>(owner_,
      [path=path_,fallback=fallbackPath_,key=std::string(key),value]{
        application_config::patchConfigSettings(path,Json{{key,value}},fallback);
        return event_loop::Unit{};
      }).then([self,key=std::string(key),value,output](
          const event_loop::Unit*,const std::exception_ptr& error){
        self->busy_=false;
        if(error){output.reject(error);return;}
        try{
          if(key=="autoCompactEnabled"){
            const bool enabled=value.get<bool>();
            self->enabled_.store(enabled,std::memory_order_release);
            self->projectEnabled(enabled);
          }else{
            const auto threshold=value.get<std::size_t>();
            self->compactAt_.store(threshold,std::memory_order_release);
            self->projectThreshold(threshold);
          }
          output.resolve({});
        }catch(...){output.reject(std::current_exception());}
      });
  }catch(...){busy_=false;output.reject(std::current_exception());}
  return output;
}
event_loop::Deferred<event_loop::Unit> Policy::setEnabled(bool value){
  return change("autoCompactEnabled",value);
}
event_loop::Deferred<event_loop::Unit> Policy::setThreshold(std::size_t value){
  if(value==0){event_loop::Deferred<event_loop::Unit> output(owner_);
    output.reject(std::make_exception_ptr(std::invalid_argument(
      "Compaction threshold must be positive")));return output;}
  return change("compact_at",value);
}
void Policy::applyExternalThreshold(std::size_t value){
  if(!owner_.isOwnerThread())throw std::logic_error(
    "External compaction change requires owner loop");
  if(value==0)throw std::invalid_argument("Compaction threshold must be positive");
  compactAt_.store(value,std::memory_order_release);
  projectThreshold(value);
}
} // namespace batbox::application_compaction_settings
