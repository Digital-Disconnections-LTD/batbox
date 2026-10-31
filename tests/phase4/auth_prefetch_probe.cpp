#include "auth/storage/KeychainPrefetch.hpp"
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
  if(!value)throw std::runtime_error("Prefetch did not settle");
  return *value;
}
namespace {
batbox::Json legacy(const std::optional<
    std::optional<std::string>>& result){
  if(!result)return nullptr;
  return batbox::Json{{"stdout",*result
    ?batbox::Json(**result):batbox::Json(nullptr)}};
}
}
int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array();
    event_loop::Loop loop;
    auth::storage::KeychainServices keychainServices;
    keychainServices.configHome=[](){return std::string("/config");};
    keychainServices.configDirOverride=[]()->std::optional<std::string>{
      return std::nullopt;};
    keychainServices.userEnv=[]()->std::optional<std::string>{
      return std::string("alice");};
    keychainServices.osUsername=[](){return std::string("system-user");};
    keychainServices.nowMs=[](){return 100000.0;};
    keychainServices.oauthFileSuffix=[](){return std::string("-oauth");};
    auto state=std::make_shared<auth::storage::KeychainState>(
      keychainServices);
    auth::storage::PrefetchServices services;
    services.platform=[&](){return test.value("platform",
      std::string("darwin"));};
    services.bareMode=[&](){trace.push_back("bare");
      return test.value("bare",false);};
    services.primeStash=[&](const std::optional<std::string>& value){
      trace.push_back(Json::array({"prime",value
        ?Json(*value):Json(nullptr)}));
      state->prime(value);
    };
    services.spawnSecurity=[&](const std::string& name){
      trace.push_back(Json::array({"spawn","security",
        Json::array({"find-generic-password","-a","alice","-w",
          "-s",name}),10000}));
      const auto oauth=name.size()>=12&&
        name.substr(name.size()-12)=="-credentials";
      const auto result=test.value(oauth?"oauth":"legacy",Json::object());
      event_loop::Deferred<auth::storage::PrefetchResult> output(loop);
      const bool error=result.value("error",false);
      const auto stdoutText=value::trim(result.value("stdout",
        std::string()));
      output.resolve({error||stdoutText.empty()?std::nullopt:
        std::optional<std::string>(stdoutText),
        error&&result.value("timedOut",false)});
      return output;
    };
    auth::storage::KeychainPrefetch prefetch(loop,state,services);
    prefetch.start();
    if(test.value("startTwice",false))prefetch.start();
    (void)await(loop,prefetch.ensureCompleted());
    const auto before=legacy(prefetch.legacyApiKeyResult());
    if(test.value("clear",false))prefetch.clearLegacyApiKeyResult();
    std::cout<<value::stringify(Json{{"before",before},
      {"after",legacy(prefetch.legacyApiKeyResult())},
      {"trace",trace}})<<'\n';
  }
}
