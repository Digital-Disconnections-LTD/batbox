#include "mcp/auth/Provider.hpp"
#include "auth/oauth_client/FormUrl.hpp"
#include "runtime/Url.hpp"
#include <stdexcept>

namespace batbox::mcp::auth {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string text(const Json& value){return value.is_string()?value.get<std::string>():std::string{};}
}
Provider::Provider(std::string name,Json config,Services services,
    std::string redirectUri,bool redirect,bool skipBrowser):name_(std::move(name)),
    redirectUri_(std::move(redirectUri)),config_(std::move(config)),
    services_(std::move(services)),handleRedirection_(redirect),skipBrowserOpen_(skipBrowser){
  if(name_.empty()||!field(config_,"url").is_string()||!services_.storage.read||
      !services_.storage.update||!services_.now||!services_.randomBase64Url)
    throw std::invalid_argument("MCP OAuth provider bindings required");
}
void Provider::debug(const std::string& message)const{
  if(services_.debug)services_.debug(message);
}
Json Provider::read()const{
  std::lock_guard lock(*services_.storageMutex);
  return services_.storage.read().value_or(Json::object());
}
Json Provider::entry()const{return value::property(field(read(),"mcpOAuth"),getServerKey(name_,config_));}
void Provider::updateEntry(const std::function<void(Json&)>& update){
  std::lock_guard storageLock(*services_.storageMutex);
  auto data=read();auto& record=data["mcpOAuth"][getServerKey(name_,config_)];
  if(!record.is_object())record=Json::object();
  update(record);services_.storage.update(data);
}
std::optional<std::string> Provider::authorizationUrl()const{
  std::lock_guard lock(mutex_);return authorizationUrl_;
}
Json Provider::clientMetadata()const{
  std::lock_guard lock(mutex_);
  Json metadata={{"client_name","Batbox ("+name_+")"},{"redirect_uris",Json::array({redirectUri_})},
    {"grant_types",Json::array({"authorization_code","refresh_token"})},
    {"response_types",Json::array({"code"})},{"token_endpoint_auth_method","none"}};
  const auto scope=getScopeFromMetadata(metadata_);
  if(!scope.empty())metadata["scope"]=scope;
  return metadata;
}
std::optional<std::string> Provider::clientMetadataUrl()const{return services_.clientMetadataUrl;}
void Provider::setMetadata(Json value){std::lock_guard lock(mutex_);metadata_=std::move(value);}
void Provider::markStepUpPending(std::string value){
  std::lock_guard lock(mutex_);pendingStepUpScope_=std::move(value);debug("Marked step-up pending");
}
std::string Provider::state(){
  std::lock_guard lock(mutex_);
  if(!state_)state_=services_.randomBase64Url(32);
  return *state_;
}
Json Provider::clientInformation(){
  std::lock_guard lock(mutex_);const auto data=read();
  const auto key=getServerKey(name_,config_);const auto& info=value::property(field(data,"mcpOAuth"),key);
  if(value::truthy(field(info,"clientId"))){
    Json result={{"client_id",field(info,"clientId")}};
    if(!field(info,"clientSecret").is_null())result["client_secret"]=field(info,"clientSecret");
    return result;
  }
  const auto& configured=field(field(config_,"oauth"),"clientId");
  if(value::truthy(configured)){
    Json result={{"client_id",configured}};
    const auto& client=value::property(field(data,"mcpOAuthClientConfig"),key);
    if(!field(client,"clientSecret").is_null())result["client_secret"]=field(client,"clientSecret");
    return result;
  }
  return Json();
}
void Provider::saveClientInformation(const Json& info){
  std::lock_guard lock(mutex_);
  updateEntry([&](Json& value){
    value["serverName"]=name_;value["serverUrl"]=config_.at("url");
    value["clientId"]=info.at("client_id");
    if(info.contains("client_secret"))value["clientSecret"]=info.at("client_secret");
    else value.erase("clientSecret");
    if(!value::truthy(field(value,"accessToken")))value["accessToken"]="";
    if(!value::truthy(field(value,"expiresAt")))value["expiresAt"]=0;
  });
}
void Provider::saveTokens(const Json& tokens){
  std::lock_guard lock(mutex_);pendingStepUpScope_.reset();
  updateEntry([&](Json& value){
    value["serverName"]=name_;value["serverUrl"]=config_.at("url");
    value["accessToken"]=tokens.at("access_token");
    for(const auto& [wire,stored]:{std::pair{"refresh_token","refreshToken"},std::pair{"scope","scope"}})
      if(tokens.contains(wire))value[stored]=tokens.at(wire);else value.erase(stored);
    const auto& expires=field(tokens,"expires_in");
    value["expiresAt"]=services_.now()+(value::truthy(expires)?value::numberOr(expires):3600)*1000;
  });
}
void Provider::redirectToAuthorization(const std::string& url){
  std::lock_guard lock(mutex_);authorizationUrl_=url;
  const auto parsed=runtime::parseUrl(url);
  if(!parsed)throw std::runtime_error("Invalid authorization URL: must use http:// or https:// scheme");
  const auto search=text(field(*parsed,"search"));
  auto entries=::batbox::auth::oauth_client::parseFormQuery(
    search.starts_with('?')?search.substr(1):search);
  std::string scope;
  for(const auto& [key,value]:entries)if(key=="scope"){scope=value;break;}
  if(!scope.empty())scopes_=scope;
  else if(const auto metadataScope=getScopeFromMetadata(metadata_);!metadataScope.empty())scopes_=metadataScope;
  if(scopes_&&!handleRedirection_){
    std::lock_guard storageLock(*services_.storageMutex);
    auto data=read();auto& records=data["mcpOAuth"];
    const auto key=getServerKey(name_,config_);
    if(records.is_object()&&records.contains(key)){
      records[key]["stepUpScope"]=*scopes_;services_.storage.update(data);
    }
  }
  if(!handleRedirection_)return;
  if(!url.starts_with("http://")&&!url.starts_with("https://"))
    throw std::runtime_error("Invalid authorization URL: must use http:// or https:// scheme");
  if(services_.authorizationUrl)services_.authorizationUrl(url);
  if(!skipBrowserOpen_&&services_.openBrowser)(void)services_.openBrowser(url);
}
void Provider::saveCodeVerifier(std::string value){std::lock_guard lock(mutex_);verifier_=std::move(value);}
std::string Provider::codeVerifier()const{
  std::lock_guard lock(mutex_);
  if(!verifier_)throw std::runtime_error("No code verifier saved");
  return *verifier_;
}
void Provider::invalidateCredentials(const std::string& scope){
  std::lock_guard lock(mutex_);std::lock_guard storageLock(*services_.storageMutex);auto data=read();
  auto& records=data["mcpOAuth"];const auto key=getServerKey(name_,config_);
  if(!records.is_object()||!records.contains(key))return;
  auto& token=records[key];
  if(scope=="all")records.erase(key);
  else if(scope=="client"){token.erase("clientId");token.erase("clientSecret");}
  else if(scope=="tokens"){token["accessToken"]="";token.erase("refreshToken");token["expiresAt"]=0;}
  else if(scope=="verifier"){verifier_.reset();return;}
  else if(scope=="discovery"){token.erase("discoveryState");token.erase("stepUpScope");}
  services_.storage.update(data);
}
void Provider::saveDiscoveryState(const Json& state){
  std::lock_guard lock(mutex_);
  updateEntry([&](Json& value){
    value["serverName"]=name_;value["serverUrl"]=config_.at("url");
    if(!value::truthy(field(value,"accessToken")))value["accessToken"]="";
    if(!value::truthy(field(value,"expiresAt")))value["expiresAt"]=0;
    Json urls={{"authorizationServerUrl",state.at("authorizationServerUrl")}};
    if(state.contains("resourceMetadataUrl"))urls["resourceMetadataUrl"]=state.at("resourceMetadataUrl");
    value["discoveryState"]=std::move(urls);
  });
}
Json Provider::discoveryState(){
  std::lock_guard lock(mutex_);const auto stashed=field(entry(),"discoveryState");
  if(value::truthy(field(stashed,"authorizationServerUrl")))return stashed;
  const auto configured=field(field(config_,"oauth"),"authServerMetadataUrl");
  if(configured.is_string()&&services_.discover){
    try{
      auto metadata=services_.discover(config_.at("url").get<std::string>(),config_,{});
      if(!metadata.is_null())return Json{{"authorizationServerUrl",metadata.at("issuer")},
        {"authorizationServerMetadata",metadata}};
    }catch(const std::exception& error){debug(error.what());}
  }
  return Json();
}
} // namespace batbox::mcp::auth
