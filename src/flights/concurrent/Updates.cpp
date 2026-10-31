#include "flights/concurrent/Internal.hpp"

namespace batbox::flights::concurrent {
Json spread(const Json& value){
  if(value.is_object())return value;
  Json result=Json::object();
  if(value.is_array()){
    for(std::size_t i=0;i<value.size();++i)result[std::to_string(i)]=value[i];
  }else if(value.is_string()){
    const auto units=value::utf16CodeUnits(value.get<std::string>());
    for(std::size_t i=0;i<units.size();++i)
      result[std::to_string(i)]=value::fromUtf16(std::u16string_view(units).substr(i,1));
  }
  return result;
}
Deferred<Unit> Roster::State::updatePidFile(Json patch){
  const auto state=shared_from_this();
  Deferred<Unit> output(*s.loop);
  // Construct the path before entering the catch block.
  std::string path;
  try{path=pidFile();}catch(...){output.reject(std::current_exception());return output;}
  const auto fail=[state,output](const std::exception_ptr& error){
    try{state->debugFailure("updatePidFile",error);output.resolve(Unit{});}
    catch(...){output.reject(std::current_exception());}
  };
  try{s.readFile(path).then([state,path,patch=std::move(patch),output,fail]
      (const std::string* text,const std::exception_ptr& error){
    if(error){fail(error);return;}
    try{
      auto merged=spread(state->s.jsonParse(*text));
      for(const auto& key:value::objectKeys(patch))merged[key]=patch[key];
      state->s.writeFile(path,state->s.jsonStringify(merged)).then(
        [output,fail](const Unit*,const std::exception_ptr& error){
          if(error)fail(error);else output.resolve(Unit{});
        });
    }catch(...){fail(std::current_exception());}
  });}catch(...){fail(std::current_exception());}
  return output;
}
Deferred<Unit> Roster::updateFlightName(std::optional<std::string> name)const{
  Deferred<Unit> output(*state_->s.loop);
  try{
    if(!name||name->empty()){output.resolve(Unit{});return output;}
    state_->updatePidFile(Json{{"name",*name}}).then(
      [output](const Unit* value,const std::exception_ptr& error){
        if(error)output.reject(error);else output.resolve(*value);
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
Deferred<Unit> Roster::updateFlightBridgeId(std::optional<std::string> id)const{
  Deferred<Unit> output(*state_->s.loop);
  try{state_->updatePidFile(Json{{"bridgeSessionId",id?Json(*id):Json()}}).then(
    [output](const Unit* value,const std::exception_ptr& error){
      if(error)output.reject(error);else output.resolve(*value);
    });}catch(...){output.reject(std::current_exception());}
  return output;
}
Deferred<Unit> Roster::updateFlightActivity(Json patch)const{
  Deferred<Unit> output(*state_->s.loop);
  try{
    if(!state_->s.feature("BG_SESSIONS")){output.resolve(Unit{});return output;}
    auto merged=spread(patch);merged["updatedAt"]=state_->s.now();
    state_->updatePidFile(std::move(merged)).then(
      [output](const Unit* value,const std::exception_ptr& error){
        if(error)output.reject(error);else output.resolve(*value);
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::flights::concurrent
