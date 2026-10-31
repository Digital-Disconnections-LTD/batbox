#include "settings/mdm/Store.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::settings::mdm {
namespace {
std::string joinKeys(const std::vector<std::string>& keys){
  std::string result;
  for(const auto& key:keys){if(!result.empty())result+=", ";result+=key;}
  return result;
}
}
Store::Store(event_loop::Loop& loop,ParseServices parser,StoreServices services):
  loop_(loop),parser_(std::move(parser)),services_(std::move(services)){
  if(!services_.startupRaw||!services_.freshRaw||
      !services_.profileCheckpoint||!services_.nowMs||!services_.debug||
      !services_.localDiagnostics)
    throw std::invalid_argument("Missing MDM store binding");
}
void Store::startMdmSettingsLoad(){
  if(!loop_.isOwnerThread())
    throw std::logic_error("MDM store requires owner event loop");
  if(loadPromise_)return;
  event_loop::Deferred<event_loop::Unit> output(loop_);
  loadPromise_=output;
  try{
    services_.profileCheckpoint("mdm_load_start");
    const auto start=services_.nowMs();
    const auto existing=services_.startupRaw();
    const auto raw=existing?*existing:services_.freshRaw();
    raw.then([this,output,start](const RawReadResult* value,
        const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      try{
        auto parsed=consumeRawReadResult(*value,parser_);
        mdmStash_=parsed.mdm;hkcuStash_=parsed.hkcu;
        services_.profileCheckpoint("mdm_load_end");
        const auto duration=services_.nowMs()-start;
        services_.debug("MDM settings load completed in "+
          value::numberToString(duration)+"ms");
        const auto keys=value::objectKeys(parsed.mdm.settings);
        if(!keys.empty()){
          services_.debug("MDM settings found: "+joinKeys(keys));
          try{
            services_.localDiagnostics("info","mdm_settings_loaded",
              Json{{"duration_ms",duration},{"key_count",keys.size()},
                {"error_count",parsed.mdm.errors.size()}});
          }catch(...){/* Diagnostics are best effort. */}
        }
        output.resolve({});
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
}
event_loop::Deferred<event_loop::Unit> Store::ensureMdmSettingsLoaded(){
  if(!loadPromise_)startMdmSettingsLoad();
  event_loop::Deferred<event_loop::Unit> output(loop_);
  loadPromise_->then([output](const event_loop::Unit* value,
      const std::exception_ptr& error){
    if(error)output.reject(error);else output.resolve(*value);
  });
  return output;
}
MdmResult Store::getMdmSettings()const{
  return mdmStash_.value_or(MdmResult{});
}
MdmResult Store::getHkcuSettings()const{
  return hkcuStash_.value_or(MdmResult{});
}
void Store::clearMdmSettingsStash(){
  mdmStash_.reset();hkcuStash_.reset();loadPromise_.reset();
}
void Store::setMdmSettingsStash(MdmResult mdm,MdmResult hkcu){
  mdmStash_=std::move(mdm);hkcuStash_=std::move(hkcu);
}
event_loop::Deferred<ParsedRaw> Store::refreshMdmSettings(){
  event_loop::Deferred<ParsedRaw> output(loop_);
  try{
    services_.freshRaw().then([this,output](const RawReadResult* value,
        const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      try{output.resolve(consumeRawReadResult(*value,parser_));}
      catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
std::optional<event_loop::Deferred<event_loop::Unit>> Store::loadPromise()const{
  return loadPromise_;
}
}
