#include "auth/storage/MacKeychain.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

template<class T>T await(batbox::event_loop::Loop& loop,
    batbox::event_loop::Deferred<T> promise){
  std::optional<T> value;std::exception_ptr failure;
  promise.then([&](const T* result,const std::exception_ptr& error){
    if(error)failure=error;else value=*result;
  });
  loop.run();
  if(failure)std::rethrow_exception(failure);
  if(!value)throw std::runtime_error("Keychain promise did not settle");
  return *value;
}
int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array();
    event_loop::Loop loop;
    auth::storage::KeychainServices services;
    services.configHome=[&](){trace.push_back("home");
      return std::string("/config");};
    services.configDirOverride=[]()->std::optional<std::string>{
      return std::nullopt;};
    services.userEnv=[&]()->std::optional<std::string>{
      return test.value("user",std::string("alice"));};
    services.osUsername=[](){return std::string("system-user");};
    services.nowMs=[&](){trace.push_back("now");
      return test.value("now",100000.0);};
    services.oauthFileSuffix=[&](){trace.push_back("oauth");
      return std::string("-oauth");};
    auto state=std::make_shared<auth::storage::KeychainState>(services);
    if(test.contains("prior")){
      if(!test["prior"].is_null())state->data=test["prior"];
      state->stashedAt=test.value("cachedAt",1.0);
    }
    auth::storage::MacKeychainOps operations;
    std::optional<event_loop::Deferred<auth::storage::SecurityResult>>
      deferredAsync;
    operations.findSync=[&](const std::string& user,
        const std::string& service){
      trace.push_back(Json::array({"findSync",user,service}));
      if(test.value("syncError",false))
        throw std::runtime_error("security failed");
      return test.value("findText",std::string());
    };
    operations.findAsync=[&](const std::string& user,
        const std::string& service){
      trace.push_back(Json::array({"findAsync",user,service}));
      event_loop::Deferred<auth::storage::SecurityResult> output(loop);
      if(test.value("deferAsync",false)){
        deferredAsync=output;return output;
      }
      if(test.value("asyncError",false))
        output.reject(std::make_exception_ptr(
          std::runtime_error("security failed")));
      else output.resolve({test.value("findCode",0),
        test.value("findText",std::string())});
      return output;
    };
    operations.executeSync=[&](const std::vector<std::string>& args,
        const std::optional<std::string>& input){
      trace.push_back(Json::array({"execSync",args,
        input?Json(*input):Json(nullptr)}));
      if(test.value("execError",false))
        throw std::runtime_error("security failed");
      return auth::storage::SecurityResult{test.value("exitCode",0),{}};
    };
    operations.deleteSync=[&](const std::string& user,
        const std::string& service){
      trace.push_back(Json::array({"deleteSync",user,service}));
      if(test.value("syncError",false))
        throw std::runtime_error("security failed");
    };
    operations.debug=[&](const std::string& message,
        const std::string& level){
      trace.push_back(Json::array({"debug",message,level}));
    };
    auto storage=auth::storage::makeMacKeychainStorage(loop,state,operations);
    Json value;
    try{
      const auto op=test.value("op",std::string());
      if(op=="read"){
        const auto read=storage.read();value=read?*read:Json(nullptr);
      }else if(op=="readAsync"){
        const auto read=await(loop,storage.readAsync());
        value=read?*read:Json(nullptr);
      }else if(op=="parallelReadAsync"){
        const auto first=storage.readAsync(),second=storage.readAsync();
        const auto a=await(loop,first),b=await(loop,second);
        value=Json::array({a?*a:Json(nullptr),b?*b:Json(nullptr)});
      }else if(op=="generationRace"){
        const auto read=storage.readAsync();
        const auto update=storage.update(test.value("data",Json::object()));
        deferredAsync->resolve({test.value("findCode",0),
          test.value("findText",std::string())});
        const auto readValue=await(loop,read);
        value=Json{{"read",readValue?*readValue:Json(nullptr)},
          {"update",update}};
      }else if(op=="update")value=storage.update(
        test.value("data",Json::object()));
      else if(op=="delete")value=storage.remove();
    }catch(const std::exception& error){value=Json{{"error",error.what()}};}
    std::cout<<value::stringify(Json{{"value",value},{"trace",trace},
      {"state",Json{{"data",state->data?*state->data:Json(nullptr)},
        {"cachedAt",state->stashedAt},{"generation",state->generation},
        {"inFlight",state->readInFlight.has_value()}}}})<<'\n';
  }
}
