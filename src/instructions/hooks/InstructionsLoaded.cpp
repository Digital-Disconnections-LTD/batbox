#include "instructions/hooks/InstructionsLoaded.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::instructions::hooks {
namespace {
bool nonempty(const std::optional<Json>& value){
  if(!value||!value->is_object())return false;
  const auto& hooks=value::property(*value,"InstructionsLoaded");
  if(hooks.is_array())return !hooks.empty();
  if(hooks.is_string())return value::stringLength(hooks.get<std::string>())>0;
  if(hooks.is_object()){
    const auto& length=value::property(hooks,"length");
    return length.is_number()&&length.get<double>()>0;
  }
  return false;
}
void setOptional(HookDispatch& dispatch,const char* name,
  const std::optional<std::string>& value){
  if(value)dispatch.hookInput[name]=*value;
  else{dispatch.hookInput.erase(name);dispatch.ownUndefinedKeys.emplace_back(name);}
}
}
bool hasInstructionsLoadedHook(const InstructionsLoadedImports& imports){
  if(!imports.getHooksConfigFromSnapshot)
    throw std::invalid_argument("InstructionsLoaded snapshot import required");
  if(nonempty(imports.getHooksConfigFromSnapshot()))return true;
  if(!imports.getRegisteredHooks)
    throw std::invalid_argument("InstructionsLoaded registration import required");
  return nonempty(imports.getRegisteredHooks());
}
event_loop::Deferred<event_loop::Unit> executeInstructionsLoadedHooks(
  event_loop::Loop& loop,const InstructionsLoadedImports& imports,
  const std::string& filePath,const std::string& memoryType,
  const std::string& loadReason,const InstructionsLoadedOptions& options){
  if(!loop.isOwnerThread())throw std::logic_error("InstructionsLoaded owner required");
  event_loop::Deferred<event_loop::Unit> output(loop);
  try{
    if(!imports.createBaseHookInput||!imports.executeHooksOutsideRepl)
      throw std::invalid_argument("InstructionsLoaded executor imports required");
    HookDispatch dispatch;
    if(!options.timeoutMs&&!imports.defaultTimeoutMs)
      throw std::invalid_argument("InstructionsLoaded timeout import required");
    dispatch.timeoutMs=options.timeoutMs?*options.timeoutMs:*imports.defaultTimeoutMs;
    dispatch.matchQuery=loadReason;
    dispatch.hookInput=imports.createBaseHookInput();
    if(!dispatch.hookInput.is_object())
      throw std::invalid_argument("Base hook input must be an object");
    auto keyOrder=value::objectKeys(dispatch.hookInput);
    for(const auto* key:{"hook_event_name","file_path","memory_type",
      "load_reason","globs","trigger_file_path","parent_file_path"}){
      if(std::find(keyOrder.begin(),keyOrder.end(),key)==keyOrder.end())
        keyOrder.emplace_back(key);
    }
    dispatch.hookInput["hook_event_name"]="InstructionsLoaded";
    dispatch.hookInput["file_path"]=filePath;
    dispatch.hookInput["memory_type"]=memoryType;
    dispatch.hookInput["load_reason"]=loadReason;
    if(options.globs)dispatch.hookInput["globs"]=*options.globs;
    else{dispatch.hookInput.erase("globs");dispatch.ownUndefinedKeys.emplace_back("globs");}
    setOptional(dispatch,"trigger_file_path",options.triggerFilePath);
    setOptional(dispatch,"parent_file_path",options.parentFilePath);
    const auto undefined=dispatch.ownUndefinedKeys;
    dispatch.ownUndefinedKeys.clear();
    for(const auto& key:keyOrder){
      if(std::find(undefined.begin(),undefined.end(),key)!=undefined.end())
        dispatch.ownUndefinedKeys.push_back(key);
    }
    imports.executeHooksOutsideRepl(std::move(dispatch)).then(
      [output](const Json*,const std::exception_ptr& error){
        if(error)output.reject(error);else output.resolve(event_loop::Unit{});
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::instructions::hooks
