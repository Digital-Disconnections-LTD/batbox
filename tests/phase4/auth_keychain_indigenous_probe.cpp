#include "auth/storage/MacKeychain.hpp"
#include "auth/storage/IndigenousSecurity.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

template<class T>T await(batbox::event_loop::Loop& loop,
    batbox::event_loop::Deferred<T> promise){
  std::optional<T> value;std::exception_ptr failure;
  promise.then([&](const T* result,const std::exception_ptr& error){
    if(error)failure=error;else value=*result;
  });
  loop.run();
  if(failure)std::rethrow_exception(failure);
  if(!value)throw std::runtime_error("Native security call did not settle");
  return *value;
}
int main(int argc,char** argv){
  if(argc>1){
    const std::string action=argv[1];
    if(action=="find-generic-password"){
      std::cout<<"{\"batboxOauth\":{\"accessToken\":\"from-security\"}}\n";
      return 0;
    }
    const auto record=std::getenv("BATBOX_SECURITY_FIXTURE_RECORD");
    assert(record);
    std::ofstream output(record,std::ios::trunc);
    output<<action<<'\n';
    if(action=="-i"){
      std::string line;
      std::getline(std::cin,line);
      output<<line<<'\n';
    }else{
      for(int index=2;index<argc;++index)output<<argv[index]<<'\n';
    }
    return 0;
  }
  using namespace batbox;
  char temporary[]="/tmp/batbox-native-security-XXXXXX";
  const char* directory=::mkdtemp(temporary);
  assert(directory);
  const auto record=std::string(directory)+"/record";
  assert(::setenv("BATBOX_SECURITY_FIXTURE_RECORD",record.c_str(),1)==0);
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
  const auto operations=auth::storage::indigenousMacKeychainOps(loop,
    [](const std::string&,const std::string&){},
    std::filesystem::canonical(argv[0]).string());
  const auto keychain=auth::storage::makeMacKeychainStorage(
    loop,state,operations);
  const auto first=keychain.read();
  assert(first&&(*first)["batboxOauth"]["accessToken"]=="from-security");
  state->clear();
  const auto asynchronous=await(loop,keychain.readAsync());
  assert(asynchronous&&*asynchronous==*first);
  const Json small={{"batboxOauth",{{"accessToken","next"}}}};
  assert(keychain.update(small)["success"]==true);
  {
    std::ifstream input(record);
    std::string firstLine,command;
    std::getline(input,firstLine);std::getline(input,command);
    assert(firstLine=="-i");
    assert(command.find("add-generic-password -U -a")!=
      std::string::npos);
    assert(command.find("next")==std::string::npos);
  }
  const Json large={{"payload",std::string(2200,'x')}};
  assert(keychain.update(large)["success"]==true);
  {
    std::ifstream input(record);
    std::string firstLine;
    std::getline(input,firstLine);
    assert(firstLine=="add-generic-password");
  }
  assert(keychain.remove());
  {
    std::ifstream input(record);
    std::string firstLine;
    std::getline(input,firstLine);
    assert(firstLine=="delete-generic-password");
  }
  std::filesystem::remove_all(directory);
  std::cout<<"Native macOS security command adapter fixture passed\n";
}
