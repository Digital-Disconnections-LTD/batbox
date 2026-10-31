#include "auth/save/ApiKeySave.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

namespace {
template<class T>T await(batbox::event_loop::Loop& loop,
    batbox::event_loop::Deferred<T> promise){
  std::optional<T> value;std::exception_ptr failure;
  promise.then([&](const T* result,const std::exception_ptr& error){
    if(error)failure=error;else value=*result;
  });
  loop.run();
  if(failure)std::rethrow_exception(failure);
  if(!value)throw std::runtime_error("API key save did not settle");
  return *value;
}
std::string message(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}
  catch(const std::exception& caught){return caught.what();}
  catch(...){return "unknown error";}
}
}
int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    event_loop::Loop loop;
    Json config=test.value("config",Json::object()),trace=Json::array();
    auth::save::ApiKeySaveServices services;
    services.platform=[&](){return test.value("platform",
      std::string("linux"));};
    services.keychainServiceName=[&](){trace.push_back("service");
      return std::string("Batbox-oauth");};
    services.username=[&](){trace.push_back("username");
      return std::string("alice");};
    services.deleteKeychainEntry=[&](const std::string& service){
      trace.push_back(Json::array({"delete",
        "security delete-generic-password -a $USER -s \""+service+"\""}));
      event_loop::Deferred<auth::storage::SecurityResult> output(loop);
      if(test.value("deleteThrow",false))output.reject(
        std::make_exception_ptr(std::runtime_error("delete failed")));
      else output.resolve({test.value("deleteCode",0),{}});
      return output;
    };
    services.addInteractiveKeychainEntry=[&](const std::string& command){
      trace.push_back(Json::array({"add","security",
        Json::array({"-i"}),command}));
      event_loop::Deferred<auth::storage::SecurityResult> output(loop);
      if(test.value("addThrow",false))output.reject(
        std::make_exception_ptr(std::runtime_error("add failed")));
      else output.resolve({test.value("addCode",0),{}});
      return output;
    };
    services.globalConfig=[&](){trace.push_back("getConfig");
      return config;};
    services.saveGlobalConfig=[&](
        const std::function<Json(const Json&)>& update){
      config=update(config);trace.push_back(Json::array({"save",config}));
      if(test.value("saveError",false))
        throw std::runtime_error("save failed");
    };
    services.clearLegacyKeyMemo=[&](){trace.push_back("clearMemo");};
    services.clearLegacyPrefetch=[&](){trace.push_back("clearPrefetch");};
    services.logError=[&](const std::exception_ptr& error){
      trace.push_back(Json::array({"error",message(error)}));};
    auth::save::ApiKeySaver saver(loop,services);
    Json value;
    try{
      const auto op=test.value("op",std::string("save"));
      if(op=="remove"){
        (void)await(loop,saver.remove());value="ok";
      }else if(op=="approved")value=saver.isApproved(
        test.value("key",std::string("key")));
      else{
        (void)await(loop,saver.save(
          test.value("key",std::string("key"))));value="ok";
      }
    }catch(const std::exception& error){value=Json{{"error",error.what()}};}
    std::cout<<value::stringify(Json{{"value",value},{"config",config},
      {"trace",trace}})<<'\n';
  }
}
