#include "auth/save/ApiKeySave.hpp"
#include "auth/save/IndigenousApiKeySave.hpp"
#include "auth/storage/IndigenousSecurity.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <unistd.h>

template<class T>T await(batbox::event_loop::Loop& loop,
    batbox::event_loop::Deferred<T> promise){
  std::optional<T> value;std::exception_ptr failure;
  promise.then([&](const T* result,const std::exception_ptr& error){
    if(error)failure=error;else value=*result;
  });
  loop.run();
  if(failure)std::rethrow_exception(failure);
  if(!value)throw std::runtime_error("API key operation did not settle");
  return *value;
}
int main(int argc,char** argv){
  if(argc>1){
    const char* record=std::getenv("BATBOX_API_SAVE_RECORD");
    assert(record);
    std::ofstream output(record,std::ios::trunc);
    output<<argv[1]<<'\n';
    if(std::string(argv[1])=="-i"){
      std::string command;std::getline(std::cin,command);
      output<<command<<'\n';
    }else for(int index=2;index<argc;++index)
      output<<argv[index]<<'\n';
    return 0;
  }
  using namespace batbox;
  char temporary[]="/tmp/batbox-api-save-XXXXXX";
  const char* directory=::mkdtemp(temporary);
  assert(directory);
  const auto record=std::string(directory)+"/record";
  assert(::setenv("BATBOX_API_SAVE_RECORD",record.c_str(),1)==0);
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
  auto operations=auth::storage::indigenousMacKeychainOps(loop,
    [](const std::string&,const std::string&){},
    std::filesystem::canonical(argv[0]).string());
  Json config=Json::object();
  int memo=0,prefetch=0;
  auth::save::ApiKeySaveServices services;
  services.platform=[](){return std::string("darwin");};
  services.globalConfig=[&](){return config;};
  services.saveGlobalConfig=[&](
      const std::function<Json(const Json&)>& update){config=update(config);};
  services.clearLegacyKeyMemo=[&](){++memo;};
  services.clearLegacyPrefetch=[&](){++prefetch;};
  services.logError=[](const std::exception_ptr&){assert(false);};
  auth::save::bindIndigenousApiKeyCommands(services,loop,state,operations);
  auth::save::ApiKeySaver saver(loop,services);
  (void)await(loop,saver.save("key_123"));
  assert(config["customApiKeyResponses"]["approved"]==
    Json::array({"key_123"}));
  assert(!config.contains("primaryApiKey"));
  assert(saver.isApproved("key_123"));
  {
    std::ifstream input(record);
    std::string action,command;
    std::getline(input,action);std::getline(input,command);
    assert(action=="-i");
    assert(command.find("key_123")==std::string::npos);
    assert(command.find("6b65795f313233")!=std::string::npos);
  }
  (void)await(loop,saver.remove());
  assert(memo==2&&prefetch==2);
  {
    std::ifstream input(record);
    std::string action;
    std::getline(input,action);
    assert(action=="delete-generic-password");
  }
  std::filesystem::remove_all(directory);
  std::cout<<"Native API key save fixture passed\n";
}
