#include "mcp/auth/Provider.hpp"
#include "mcp/auth/TokenSchema.hpp"
#include "auth/oauth_client/FormUrl.hpp"
#include "runtime/Url.hpp"
#include "runtime/ArrayJoin.hpp"
#include <openssl/evp.h>
#include <array>
#include <algorithm>
#include <set>
#include <stdexcept>

namespace batbox::mcp::auth {
namespace {
Json read(const ::batbox::auth::storage::Storage& storage){
  if(!storage.read||!storage.update)throw std::invalid_argument("MCP credential storage required");
  return storage.read().value_or(Json::object());
}
const Json& property(const Json& value,const char* key){return value::property(value,key);}
}
OAuthError::OAuthError(std::string value,std::string message,int httpStatus):
  std::runtime_error(std::move(message)),code(std::move(value)),status(httpStatus){}
std::string getServerKey(const std::string& name,const Json& config){
  const Json headers=value::truthy(property(config,"headers"))?property(config,"headers"):Json::object();
  Json key=Json::object();
  if(config.contains("type"))key["type"]=config.at("type");
  if(config.contains("url"))key["url"]=config.at("url");
  key["headers"]=headers;
  const auto bytes=value::stringify(key);
  std::array<unsigned char,EVP_MAX_MD_SIZE> digest{};unsigned int length=0;
  if(EVP_Digest(bytes.data(),bytes.size(),digest.data(),&length,EVP_sha256(),nullptr)!=1)
    throw std::runtime_error("MCP credential key digest failed");
  static constexpr char alphabet[]="0123456789abcdef";
  std::string hash;hash.reserve(16);
  for(std::size_t i=0;i<8;++i){hash+=alphabet[digest[i]>>4];hash+=alphabet[digest[i]&15];}
  return name+'|'+hash;
}
std::string getScopeFromMetadata(const Json& metadata){
  if(!metadata.is_object())return {};
  for(const auto* key:{"scope","default_scope"}){
    const auto& value=property(metadata,key);
    if(value.is_string())return value.get<std::string>();
  }
  const auto& scopes=property(metadata,"scopes_supported");
  if(!scopes.is_array())return {};
  return value::arrayJoin(scopes," ");
}
std::string redactSensitiveUrlParams(const std::string& url){
  const auto parsed=runtime::parseUrl(url);if(!parsed)return url;
  const auto href=parsed->at("href").get<std::string>();
  const auto question=href.find('?');if(question==std::string::npos)return href;
  const auto fragment=href.find('#',question);
  auto query=::batbox::auth::oauth_client::parseFormQuery(
    std::string_view(href).substr(question+1,fragment==std::string::npos?
      std::string::npos:fragment-question-1));
  static const std::set<std::string> sensitive={"state","nonce","code_challenge","code_verifier","code"};
  bool changed=false;
  for(const auto& key:sensitive){
    bool first=true;
    query.erase(std::remove_if(query.begin(),query.end(),[&](auto& entry){
      if(entry.first!=key)return false;
      changed=true;if(!first)return true;
      first=false;entry.second="[REDACTED]";return false;
    }),query.end());
  }
  if(!changed)return href;
  return href.substr(0,question)+'?'+::batbox::auth::oauth_client::serializeFormQuery(query)+
    (fragment==std::string::npos?std::string{}:href.substr(fragment));
}
http::Response normalizeOAuthErrorBody(http::Response response){
  if(!response.ok())return response;
  const auto parsed=Json::parse(response.body,nullptr,false);
  if(!parsed.is_object())return response;
  if(parseOAuthTokens(parsed))return response;
  const auto& error=property(parsed,"error");if(!error.is_string())return response;
  Json normalized={{"error",error}};
  for(const auto* key:{"error_description","error_uri"})if(parsed.contains(key)){
    if(!parsed.at(key).is_string())return response;
    normalized[key]=parsed.at(key);
  }
  if(error=="invalid_refresh_token"||error=="expired_refresh_token"||error=="token_expired"){
    normalized=Json{{"error","invalid_grant"},{"error_description",
      property(parsed,"error_description").is_null()?
        Json("Server returned non-standard error code: "+error.get<std::string>()):
        property(parsed,"error_description")}};
  }
  response.status=400;response.body=value::stringify(normalized);return response;
}
std::string buildRedirectUri(int port){return "http://localhost:"+std::to_string(port)+"/callback";}
bool hasMcpDiscoveryButNoToken(const std::string& name,const Json& config,
    const ::batbox::auth::storage::Storage& storage){
  const auto data=read(storage);const auto& entry=value::property(property(data,"mcpOAuth"),getServerKey(name,config));
  return !entry.is_null()&&!entry.is_discarded()&&!value::truthy(property(entry,"accessToken"))&&
    !value::truthy(property(entry,"refreshToken"));
}
void clearServerTokensFromLocalStorage(const std::string& name,const Json& config,
    const ::batbox::auth::storage::Storage& storage){
  auto data=read(storage);auto& entries=data["mcpOAuth"];
  if(entries.is_object()&&entries.erase(getServerKey(name,config)))storage.update(data);
}
void saveMcpClientSecret(const std::string& name,const Json& config,const std::string& secret,
    const ::batbox::auth::storage::Storage& storage){
  auto data=read(storage);data["mcpOAuthClientConfig"][getServerKey(name,config)]=
    Json{{"clientSecret",secret}};storage.update(data);
}
void clearMcpClientConfig(const std::string& name,const Json& config,
    const ::batbox::auth::storage::Storage& storage){
  auto data=read(storage);auto& entries=data["mcpOAuthClientConfig"];
  if(entries.is_object()&&entries.erase(getServerKey(name,config)))storage.update(data);
}
Json getMcpClientConfig(const std::string& name,const Json& config,
    const ::batbox::auth::storage::Storage& storage){
  return value::property(property(read(storage),"mcpOAuthClientConfig"),getServerKey(name,config));
}
} // namespace batbox::mcp::auth
