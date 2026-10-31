#include "auth/refresh/Check.hpp"
#include "auth/refresh/Invalidate.hpp"
#include "auth/refresh/LockBinding.hpp"
#include "locking/ProperLock.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

int main(){
  using namespace batbox;
  char temporary[]="/tmp/batbox-check-lock-XXXXXX";
  const char* home=::mkdtemp(temporary);
  assert(home);
  event_loop::Loop loop;
  locking::Manager locks(loop);
  Json token={{"accessToken","old"},{"refreshToken","refresh"},
    {"expiresAt",0},{"scopes",Json::array({"user:inference"})}};
  int refreshed=0,saved=0;
  auth::refresh::InvalidateServices invalidationServices;
  invalidationServices.configHome=[&](){return std::string(home);};
  invalidationServices.statMtimeMs=[&](const std::string& path){
    return auth::refresh::indigenousStatMtimeMs(loop,path);};
  invalidationServices.clearFullStash=[](){};
  invalidationServices.clearMemoOnly=[](){};
  auth::refresh::DiskInvalidation invalidation(loop,invalidationServices);
  auth::refresh::CheckServices services;
  services.invalidateDiskStash=[&](){
    return invalidation.invalidateIfDiskChanged();};
  services.stashedTokens=[&]()->std::optional<Json>{return token;};
  services.clearTokenMemo=[](){};
  services.clearKeychainStash=[](){};
  services.readTokensAsync=[&](){
    event_loop::Deferred<std::optional<Json>> output(loop);
    output.resolve(token);return output;};
  services.configHome=[&](){return std::string(home);};
  services.refreshToken=[&](const std::string& refreshToken,
      const std::optional<std::vector<std::string>>& scopes){
    assert(refreshToken=="refresh"&&!scopes);
    ++refreshed;
    event_loop::Deferred<Json> output(loop);
    output.resolve(Json{{"accessToken","new"},
      {"refreshToken","next"},{"expiresAt",9999999999999.0},
      {"scopes",Json::array({"user:inference"})}});
    return output;
  };
  services.saveTokens=[&](const Json& next){++saved;token=next;};
  services.logError=[](const std::string& message){
    throw std::runtime_error(message);};
  auth::refresh::bindIndigenousRefreshInfrastructure(services,loop,locks);
  auth::refresh::CheckController check(loop,services);
  bool done=false,changed=false;
  check.check().then([&](const bool* value,
      const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    changed=*value;done=true;
  });
  loop.run();
  assert(done&&changed&&refreshed==1&&saved==1);
  assert(!std::filesystem::exists(std::string(home)+".lock"));
  assert(::rmdir(home)==0);
  std::cout<<"Native OAuth refresh lock binding fixture passed\n";
}
