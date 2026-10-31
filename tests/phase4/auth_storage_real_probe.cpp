#include <cstdlib>
#include "auth/storage/AuthBinding.hpp"
#include "auth/storage/Storage.hpp"
#include "auth/save/SaveTokens.hpp"
#include "runtime/JsonValue.hpp"
#include <cassert>
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
  if(!value)throw std::runtime_error("Storage did not settle");
  return *value;
}
int main(){
  using namespace batbox;
  char temporary[]="/tmp/batbox-storage-real-XXXXXX";
  const char* root=::mkdtemp(temporary);
  assert(root);
  const std::string home=std::string(root)+"/config";
  const std::string file=home+"/.batbox-credentials.json";
  event_loop::Loop loop;
  const auto plain=auth::storage::makePlaintextStorage(loop,
    auth::storage::indigenousPlaintextServices(loop,[&]{return home;}));
  const auto selected=auth::storage::selectStorage(loop,"linux",plain,
    std::nullopt);
  assert(selected.name=="plaintext");
  assert(!selected.read());
  auth::OAuthTokenServices tokens;
  auth::save::Services save;
  int memo=0,betas=0,schemas=0;
  save.clearTokenStash=[&]{++memo;};
  save.clearBetasStashes=[&]{++betas;};
  save.clearGadgetSchemaStash=[&]{++schemas;};
  save.logError=[](const std::exception_ptr&){assert(false);};
  auth::storage::bindOAuthStorage(tokens,save,selected);
  const Json first={{"accessToken","access"},{"refreshToken","refresh"},
    {"expiresAt",999999},{"scopes",Json::array({"user:inference"})},
    {"subscriptionType","standard"}};
  const auto status=auth::save::saveOAuthTokensIfNeeded(first,save);
  assert(status["success"]==true);
  assert(memo==1&&betas==1&&schemas==1);
  assert(std::filesystem::exists(file));
  struct stat metadata{};
  assert(::stat(file.c_str(),&metadata)==0);
  assert((metadata.st_mode&0777)==0600);
  const auto stored=tokens.secureStorageRead();
  assert(stored&&(*stored)["batboxOauth"]["accessToken"]=="access");
  const auto asynchronous=await(loop,tokens.secureStorageReadAsync());
  assert(asynchronous&&*asynchronous==*stored);
  const Json second={{"accessToken","next"},{"refreshToken","next-refresh"},
    {"expiresAt",999999},{"scopes",Json::array({"user:inference"})},
    {"subscriptionType",nullptr}};
  assert(auth::save::saveOAuthTokensIfNeeded(second,save)["success"]==true);
  const auto merged=selected.read();
  assert(merged&&(*merged)["batboxOauth"]["subscriptionType"]=="standard");
  assert((*merged)["batboxOauth"]["accessToken"]=="next");
  assert(selected.remove());
  assert(selected.remove());
  assert(!selected.read());
  std::ofstream(file)<<"not json";
  assert(!selected.read());
  assert(!await(loop,selected.readAsync()));
  assert(selected.remove());
  std::filesystem::remove_all(root);
  std::cout<<"Native secure-storage file fixture passed\n";
}
