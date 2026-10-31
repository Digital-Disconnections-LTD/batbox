#include "auth/storage/KeychainPrefetch.hpp"
#include "auth/storage/IndigenousPrefetch.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <optional>
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
int main(int argc,char** argv){
  if(argc>1){
    const std::string service=argv[argc-1];
    if(service.size()>=12&&
        service.substr(service.size()-12)=="-credentials")
      std::cout<<"{\"batboxOauth\":{\"accessToken\":\"prefetched\"}}\n";
    else std::cout<<"legacy-key\n";
    return 0;
  }
  using namespace batbox;
  event_loop::Loop loop;
  auth::storage::KeychainServices services;
  services.configHome=[](){return std::string("/config");};
  services.configDirOverride=[]()->std::optional<std::string>{
    return std::nullopt;};
  services.userEnv=[]()->std::optional<std::string>{
    return std::string("alice");};
  services.osUsername=[](){return std::string("system-user");};
  services.nowMs=[](){return 100000.0;};
  services.oauthFileSuffix=[](){return std::string("-oauth");};
  auto state=std::make_shared<auth::storage::KeychainState>(services);
  const auto indigenous=auth::storage::indigenousPrefetchServices(loop,state,
    [](){return std::string("darwin");},[](){return false;},
    std::filesystem::canonical(argv[0]).string());
  auth::storage::KeychainPrefetch prefetch(loop,state,indigenous);
  prefetch.start();prefetch.start();
  (void)await(loop,prefetch.ensureCompleted());
  assert(state->data&&(*state->data)["batboxOauth"]["accessToken"]==
    "prefetched");
  const auto legacy=prefetch.legacyApiKeyResult();
  assert(legacy&&*legacy&&**legacy=="legacy-key");
  prefetch.clearLegacyApiKeyResult();
  assert(!prefetch.legacyApiKeyResult());
  std::cout<<"Native keychain prefetch fixture passed\n";
}
