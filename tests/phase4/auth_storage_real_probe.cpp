#include <cstdlib>
#include "auth/storage/Storage.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

void require(bool value,const char* message){
  if(!value)throw std::runtime_error(message);
}
template<class T>T await(batbox::event_loop::Loop& loop,
    batbox::event_loop::Deferred<T> promise){
  std::optional<T> value;std::exception_ptr failure;
  promise.then([&](const T* result,const std::exception_ptr& error){
    if(error)failure=error;else value=*result;
  });
  loop.run();
  if(failure)std::rethrow_exception(failure);
  if(!value)throw std::runtime_error("Storage did not settle");
  return *value;
}
int main(){
  using namespace batbox;
  char temporary[]="/tmp/batbox-storage-real-XXXXXX";
  const char* root=::mkdtemp(temporary);
  require(root,"root");
  const std::string home=std::string(root)+"/config";
  const std::string file=home+"/.batbox-credentials.json";
  event_loop::Loop loop;
  const auto plain=auth::storage::makePlaintextStorage(loop,
    auth::storage::indigenousPlaintextServices(loop,[&]{return home;}));
  const auto selected=auth::storage::selectStorage(loop,"linux",plain,
    std::nullopt);
  require(selected.name=="plaintext","selected.name=='plaintext'");
  require(!selected.read(),"!selected.read()");
  const Json first={{"mcpOAuth",{{"server|1",{{"accessToken","access"},
    {"refreshToken","refresh"},{"expiresAt",999999}}}}}};
  const auto status=selected.update(first);
  require(status["success"]==true,"status['success']==true");
  require(std::filesystem::exists(file),"std::filesystem::exists(file)");
  struct stat metadata{};
  require(::stat(file.c_str(),&metadata)==0,"::stat(file.c_str(),&metadata)==0");
  require((metadata.st_mode&0777)==0600,"(metadata.st_mode&0777)==0600");
  const auto stored=selected.read();
  require(stored&&(*stored)["mcpOAuth"]["server|1"]["accessToken"]=="access","stored&&(*stored)['mcpOAuth']['server|1']['accessToken']=='access'");
  const auto asynchronous=await(loop,selected.readAsync());
  require(asynchronous&&*asynchronous==*stored,"asynchronous&&*asynchronous==*stored");
  Json second=*stored;
  second["mcpOAuth"]["server|1"]["accessToken"]="next";
  require(selected.update(second)["success"]==true,"selected.update(second)['success']==true");
  const auto replaced=selected.read();
  require(replaced&&(*replaced)["mcpOAuth"]["server|1"]["accessToken"]=="next","replaced&&(*replaced)['mcpOAuth']['server|1']['accessToken']=='next'");
  require((*replaced)["mcpOAuth"]["server|1"]["refreshToken"]=="refresh","(*replaced)['mcpOAuth']['server|1']['refreshToken']=='refresh'");
  require(selected.remove(),"selected.remove()");
  require(selected.remove(),"selected.remove()");
  require(!selected.read(),"!selected.read()");
  std::ofstream(file)<<"not json";
  require(!selected.read(),"!selected.read()");
  require(!await(loop,selected.readAsync()),"!await(loop,selected.readAsync())");
  require(selected.remove(),"selected.remove()");
  std::filesystem::remove_all(root);
  std::cout<<"Native secure-storage file fixture passed\n";
}
