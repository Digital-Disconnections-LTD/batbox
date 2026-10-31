#include "mcp/auth/IndigenousServices.hpp"
#include "application/Async.hpp"
#include "auth/storage/IndigenousSecurity.hpp"
#include "process/ExecFileNoThrow.hpp"
#include "runtime/Platform.hpp"
#include "runtime/IndigenousPath.hpp"
#include <cctype>
#include <cstdlib>
#include <thread>
#include <map>

namespace batbox::mcp::auth {
struct IndigenousCredentialState {
  std::shared_ptr<std::recursive_mutex> mutex=std::make_shared<std::recursive_mutex>();
  std::shared_ptr<::batbox::auth::storage::KeychainState> keychain;
};
namespace {
std::shared_ptr<IndigenousCredentialState> credentialState(const std::filesystem::path& directory){
  static std::mutex mutex;
  static std::map<std::string,std::weak_ptr<IndigenousCredentialState>> states;
  std::lock_guard lock(mutex);
  const auto key=runtime::pathToUtf8(std::filesystem::absolute(directory).lexically_normal());
  auto state=states[key].lock();
  if(!state){state=std::make_shared<IndigenousCredentialState>();states[key]=state;}
  return state;
}
struct HeldLock {
  event_loop::Loop::Dispatcher owner;
  std::function<void()> release;
  ~HeldLock(){
    try{if(release&&owner.valid())application::onOwnerAndWait<void>(owner,release);}catch(...){}
  }
};
std::string environment(const char* key){const auto* value=std::getenv(key);return value?value:"";}
}
IndigenousServices makeIndigenousServices(event_loop::Loop& loop,std::filesystem::path directory,
    std::function<void(const std::string&)> debug,http::Fetch fetch){
  IndigenousServices result;result.locks=std::make_shared<locking::Manager>(loop);
  result.state=credentialState(directory);
  auto& services=result.services;
  services.storageMutex=result.state->mutex;
  if(!debug)debug=[](const std::string&){};
  const auto configHome=[directory]{return runtime::pathToUtf8(directory);};
  auto plaintext=::batbox::auth::storage::makePlaintextStorage(loop,
    ::batbox::auth::storage::indigenousPlaintextServices(loop,configHome));
  services.storage=plaintext;
#if defined(__APPLE__)
  ::batbox::auth::storage::KeychainServices keychain;
  keychain.configHome=configHome;
  keychain.configDirOverride=[directory]{return std::optional(runtime::pathToUtf8(directory));};
  keychain.userEnv=[]()->std::optional<std::string>{const auto user=environment("USER");
    return user.empty()?std::nullopt:std::optional(user);};
  keychain.osUsername=[] {return environment("USER");};
  keychain.nowMs=[] {return static_cast<double>(platform::nowMillis());};
  keychain.oauthFileSuffix=[] {return std::string("-mcp");};
  {std::lock_guard lock(*services.storageMutex);
    if(!result.state->keychain)result.state->keychain=std::make_shared<::batbox::auth::storage::KeychainState>(std::move(keychain));
    result.keychain=result.state->keychain;}
  auto operations=::batbox::auth::storage::indigenousMacKeychainOps(loop,
    [debug](const std::string& scope,const std::string& text){debug(scope+": "+text);});
  services.storage=::batbox::auth::storage::selectStorage(loop,"darwin",std::move(plaintext),
    ::batbox::auth::storage::makeMacKeychainStorage(loop,result.keychain,std::move(operations)));
#endif
  const auto raw=services.storage;const auto mutex=services.storageMutex;
  services.storage.read=[raw,mutex]{std::lock_guard lock(*mutex);return raw.read();};
  services.storage.update=[raw,mutex](const Json& data){std::lock_guard lock(*mutex);return raw.update(data);};
  services.storage.remove=[raw,mutex]{std::lock_guard lock(*mutex);return raw.remove();};
  services.now=[] {return static_cast<double>(platform::nowMillis());};
  services.randomBase64Url=randomBase64Url;services.random=platform::randomUnitInterval;
  services.debug=debug;services.fetch=fetch;
  services.sleep=[](double milliseconds){std::this_thread::sleep_for(std::chrono::duration<double,std::milli>(milliseconds));};
  services.clearStorageStash=[keychain=result.keychain,mutex]{
    std::lock_guard lock(*mutex);if(keychain)keychain->clear();};
  services.discover=[fetch](const std::string& url,const Json& config,const std::optional<Json>& resource){
    std::optional<std::string> location;
    if(resource&&resource->is_string())location=resource->get<std::string>();
    return fetchAuthServerMetadata(url,config,location,fetch);
  };
  services.discoverAuthorizationServer=[fetch](const std::string& url){
    return discoverAuthorizationServerMetadata(url,fetch);};
  services.refresh=[fetch](const Json& metadata,const Json& client,const std::string& token,
      const std::optional<std::string>& resource){
    Json form={{"grant_type","refresh_token"},{"refresh_token",token}};
    if(resource)form["resource"]=*resource;
    auto refreshed=exchangeToken(metadata.at("token_endpoint").get<std::string>(),std::move(form),client,fetch,{},metadata);
    // The SDK retains a nonrotated refresh token when the token endpoint
    // only returns a new access token.
    if(!refreshed.contains("refresh_token"))refreshed["refresh_token"]=token;
    return refreshed;
  };
  const auto owner=loop.dispatcher();
  services.lockRefresh=[owner,locks=result.locks,directory](const std::string& key)->std::shared_ptr<void>{
    std::string sanitized=key;
    for(auto& character:sanitized)if(!std::isalnum(static_cast<unsigned char>(character)))character='_';
    try{
      return application::onOwnerAndWait<std::shared_ptr<void>>(owner,[locks,directory,sanitized,owner]{
        std::filesystem::create_directories(directory);
        locking::Options options;options.realpath=false;
        options.onCompromised=[](const locking::Error&){};
        auto held=std::make_shared<HeldLock>();held->owner=owner;
        held->release=locks->lockSync(runtime::pathToUtf8(directory/("mcp-refresh-"+sanitized+".lock")),options);
        return held;
      });
    }catch(const locking::Error& error){throw OAuthError(error.code,error.what());}
  };
  services.openBrowser=[&loop](const std::string& url){return openBrowser(loop,url);};
  const auto document=environment("MCP_OAUTH_CLIENT_METADATA_URL");
  if(!document.empty())services.clientMetadataUrl=document;
  return result;
}
bool openBrowser(event_loop::Loop& loop,const std::string& url){
  try{
    const auto owner=loop.dispatcher();
    const auto execute=process::indigenousExeca(loop);
    const auto result=application::awaitOnOwner<process::ExecaResult>(owner,[execute,url]{
      // Constructed on the indigenous owner: process callbacks never bind to a
      // second hidden event loop.
      process::ExecaRequest request;
#if defined(_WIN32)
      request.file="rundll32.exe";request.args={"url.dll,FileProtocolHandler",url};
#elif defined(__APPLE__)
      request.file="open";request.args={url};
#else
      request.file="xdg-open";request.args={url};
#endif
      request.options.timeout=10000;
      return execute(std::move(request));
    });
    return !result.failed&&result.exitCode.value_or(1)==0;
  }catch(...){return false;}
}
} // namespace batbox::mcp::auth
