#include "mcp/auth/Discovery.hpp"
#include "runtime/Encoding.hpp"
#include <openssl/evp.h>
#include <algorithm>

namespace batbox::mcp::auth {
namespace {
std::string text(const Json& value,const char* key){const auto& item=value::property(value,key);
  return item.is_string()?item.get<std::string>():std::string{};}
std::string component(const std::string& value){
  static constexpr char hex[]="0123456789ABCDEF";
  std::string output;
  for(const unsigned char byte:value::encodeUtf8(value)){
    if((byte>='a'&&byte<='z')||(byte>='A'&&byte<='Z')||(byte>='0'&&byte<='9')||
        std::string_view("-_.!~*'()").find(static_cast<char>(byte))!=std::string_view::npos)
      output+=static_cast<char>(byte);
    else{output+='%';output+=hex[byte>>4];output+=hex[byte&15];}
  }
  return output;
}
void revokeToken(const std::string& endpoint,const std::string& token,const std::string& hint,
    const Json& record,const std::string& method,const Services& services){
  Json parameters={{"token",token},{"token_type_hint",hint}};
  http::Request request;request.method="POST";request.url=endpoint;
  request.headers={{"Content-Type","application/x-www-form-urlencoded"}};
  const auto client=text(record,"clientId"),secret=text(record,"clientSecret");
  if(!client.empty()&&!secret.empty()){
    if(method=="client_secret_post"){parameters["client_id"]=client;parameters["client_secret"]=secret;}
    else{
      const auto bytes=component(client)+':'+component(secret);
      std::string encoded(4*((bytes.size()+2)/3),'\0');
      const int size=EVP_EncodeBlock(reinterpret_cast<unsigned char*>(encoded.data()),
        reinterpret_cast<const unsigned char*>(bytes.data()),static_cast<int>(bytes.size()));
      if(size<0)throw std::runtime_error("OAuth revocation client encoding failed");
      encoded.resize(static_cast<std::size_t>(size));request.headers["Authorization"]="Basic "+encoded;
    }
  }else if(!client.empty())parameters["client_id"]=client;
  request.body=http::form(parameters);auto response=services.fetch(request);
  const auto access=text(record,"accessToken");
  if(response.status==401&&!access.empty()){
    parameters.erase("client_id");parameters.erase("client_secret");
    request.headers["Authorization"]="Bearer "+access;request.body=http::form(parameters);
    response=services.fetch(request);
  }
  if(!response.ok())throw OAuthError("revocation_failed","OAuth revocation failed",response.status);
}
}
void revokeServerTokens(const std::string& name,const Json& config,const Services& services,bool preserve){
  Json record;
  {
    std::lock_guard lock(*services.storageMutex);
    const auto data=services.storage.read().value_or(Json::object());
    if(!value::property(data,"mcpOAuth").is_object())return;
    record=value::property(data.at("mcpOAuth"),getServerKey(name,config));
  }
  const auto access=text(record,"accessToken"),refresh=text(record,"refreshToken");
  if(!access.empty()||!refresh.empty())try{
    const auto& discovery=value::property(record,"discoveryState");
    auto issuer=text(discovery,"authorizationServerUrl");if(issuer.empty())issuer=config.at("url").get<std::string>();
    const auto metadata=fetchAuthServerMetadata(issuer,config,{},services.fetch);
    const auto endpoint=text(metadata,"revocation_endpoint");
    if(!endpoint.empty()){
      auto methods=value::property(metadata,"revocation_endpoint_auth_methods_supported");
      if(methods.is_null())methods=value::property(metadata,"token_endpoint_auth_methods_supported");
      const auto includes=[&](const std::string& method){return methods.is_array()&&std::find(methods.begin(),methods.end(),Json(method))!=methods.end();};
      const std::string method=!includes("client_secret_basic")&&includes("client_secret_post")?"client_secret_post":"client_secret_basic";
      for(const auto& [token,hint]:{std::pair{refresh,"refresh_token"},std::pair{access,"access_token"}})
        if(!token.empty())try{revokeToken(endpoint,token,hint,record,method,services);}
          catch(const std::exception&){if(services.debug)services.debug("OAuth token revocation failed");}
    }
  }catch(const std::exception&){if(services.debug)services.debug("OAuth token revocation unavailable");}
  std::lock_guard lock(*services.storageMutex);
  clearServerTokensFromLocalStorage(name,config,services.storage);
  if(preserve&&(value::truthy(value::property(record,"stepUpScope"))||value::truthy(value::property(record,"discoveryState")))){
    auto data=services.storage.read().value_or(Json::object());
    auto& entry=data["mcpOAuth"][getServerKey(name,config)];if(!entry.is_object())entry=Json::object();
    entry["serverName"]=name;entry["serverUrl"]=config.at("url");
    if(!entry.contains("accessToken"))entry["accessToken"]="";
    if(!entry.contains("expiresAt"))entry["expiresAt"]=0;
    if(value::truthy(value::property(record,"stepUpScope")))entry["stepUpScope"]=record.at("stepUpScope");
    const auto& discovery=value::property(record,"discoveryState");
    if(value::truthy(discovery)){
      entry["discoveryState"]=Json::object();
      for(const auto* key:{"authorizationServerUrl","resourceMetadataUrl"})
        if(discovery.contains(key))entry["discoveryState"][key]=discovery.at(key);
    }
    services.storage.update(data);
  }
}
} // namespace batbox::mcp::auth
