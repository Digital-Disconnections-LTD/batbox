#pragma once
#include "auth/storage/Storage.hpp"
#include "mcp/http/Http.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace batbox::mcp::auth {
struct Services {
  ::batbox::auth::storage::Storage storage;
  std::shared_ptr<std::recursive_mutex> storageMutex=std::make_shared<std::recursive_mutex>();
  http::Fetch fetch=http::fetch;
  std::function<double()> now;
  std::function<std::string(std::size_t)> randomBase64Url;
  std::function<void(const std::string&)> debug;
  std::function<void()> clearStorageStash;
  std::function<Json(const std::string&,const Json&,const std::optional<Json>&)> discover;
  std::function<Json(const std::string&)> discoverAuthorizationServer;
  std::function<Json(const Json&,const Json&,const std::string&,
    const std::optional<std::string>&)> refresh;
  // Indigenous bindings implement the cross-process refresh lock.
  std::function<std::shared_ptr<void>(const std::string&)> lockRefresh;
  std::function<void(double)> sleep;
  std::function<double()> random;
  std::function<void(const std::string&)> authorizationUrl;
  std::function<bool(const std::string&)> openBrowser;
  std::optional<std::string> clientMetadataUrl;
};
class OAuthError:public std::runtime_error {
public:
  std::string code;
  int status;
  OAuthError(std::string code,std::string message,int status=0);
};
class AuthenticationCancelledError:public std::runtime_error {
public:AuthenticationCancelledError():std::runtime_error("Authentication was cancelled"){}
};
std::string getServerKey(const std::string& name,const Json& config);
std::string getScopeFromMetadata(const Json& metadata);
std::string redactSensitiveUrlParams(const std::string& url);
http::Response normalizeOAuthErrorBody(http::Response response);
std::string buildRedirectUri(int port=3118);
bool hasMcpDiscoveryButNoToken(const std::string& name,const Json& config,
  const ::batbox::auth::storage::Storage& storage);
void clearServerTokensFromLocalStorage(const std::string& name,const Json& config,
  const ::batbox::auth::storage::Storage& storage);
void revokeServerTokens(const std::string& name,const Json& config,
  const Services& services,bool preserveStepUpState=false);
void saveMcpClientSecret(const std::string& name,const Json& config,
  const std::string& secret,const ::batbox::auth::storage::Storage& storage);
void clearMcpClientConfig(const std::string& name,const Json& config,
  const ::batbox::auth::storage::Storage& storage);
Json getMcpClientConfig(const std::string& name,const Json& config,
  const ::batbox::auth::storage::Storage& storage);

class Provider {
public:
  Provider(std::string name,Json config,Services services,
    std::string redirectUri=buildRedirectUri(),bool handleRedirection=false,
    bool skipBrowserOpen=false);
  const std::string& redirectUrl()const{return redirectUri_;}
  std::optional<std::string> authorizationUrl()const;
  Json clientMetadata()const;
  std::optional<std::string> clientMetadataUrl()const;
  void setMetadata(Json metadata);
  void markStepUpPending(std::string scope);
  std::string state();
  Json clientInformation();
  void saveClientInformation(const Json& information);
  Json tokens();
  void saveTokens(const Json& tokens);
  void redirectToAuthorization(const std::string& url);
  void saveCodeVerifier(std::string verifier);
  std::string codeVerifier()const;
  void invalidateCredentials(const std::string& scope);
  void saveDiscoveryState(const Json& state);
  Json discoveryState();
  Json refreshAuthorization(const std::string& token);
  Json refreshRejectedAccessToken();
  const Json& config()const{return config_;}
  const std::string& name()const{return name_;}
  const Services& services()const{return services_;}
private:
  std::string name_,redirectUri_;
  Json config_,metadata_;
  Services services_;
  bool handleRedirection_,skipBrowserOpen_;
  std::optional<std::string> verifier_,authorizationUrl_,state_,scopes_,pendingStepUpScope_;
  mutable std::recursive_mutex mutex_;
  Json read()const;
  Json entry()const;
  void updateEntry(const std::function<void(Json&)>& update);
  Json doRefresh(const std::string& token);
  void debug(const std::string& message)const;
};
} // namespace batbox::mcp::auth
