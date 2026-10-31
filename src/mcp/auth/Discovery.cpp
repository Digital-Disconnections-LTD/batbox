#include "mcp/auth/Discovery.hpp"
#include "mcp/auth/TokenSchema.hpp"
#include "auth/oauth_client/FormUrl.hpp"
#include "runtime/Url.hpp"
#include <openssl/evp.h>
#include <algorithm>
#include <set>
#include <stdexcept>

namespace batbox::mcp::auth {
namespace {
const Json& field(const Json& value,const char* name){return value::property(value,name);}
std::string text(const Json& value){return value.is_string()?value.get<std::string>():std::string{};}
Json getMetadata(const std::string& url,const http::Fetch& fetch,std::stop_token signal){
  http::Request request;request.url=url;request.signal=signal;
  request.headers={{"Accept","application/json"},{"MCP-Protocol-Version","2025-03-26"}};
  const auto response=fetch(request);
  if(response.status==404||response.status==405)return Json();
  if(!response.ok())throw OAuthError("metadata_discovery_failed",
    "HTTP "+std::to_string(response.status)+" fetching OAuth metadata",response.status);
  auto parsed=response.json();
  if(!parsed.is_object())throw std::runtime_error("OAuth metadata must be an object");
  return parsed;
}
std::string origin(const std::string& url){
  const auto parsed=runtime::parseUrl(url);
  if(!parsed)throw std::runtime_error("Invalid MCP OAuth URL");
  return parsed->at("origin").get<std::string>();
}
std::vector<std::string> metadataLocations(const std::string& url,const std::string& kind){
  const auto parsed=runtime::parseUrl(url);
  if(!parsed)throw std::runtime_error("Invalid OAuth issuer URL");
  const auto root=parsed->at("origin").get<std::string>();
  const auto path=parsed->at("pathname").get<std::string>();
  std::vector<std::string> result;
  if(path!="/")result.push_back(root+"/.well-known/"+kind+path);
  result.push_back(root+"/.well-known/"+kind);
  if(path!="/")result.push_back(root+path+(path.ends_with('/')?"":"/")+".well-known/"+kind);
  return result;
}
}
Json discoverAuthorizationServerMetadata(const std::string& issuer,
    const http::Fetch& fetch,std::stop_token signal){
  for(const auto& kind:{"oauth-authorization-server","openid-configuration"})
    for(const auto& url:metadataLocations(issuer,kind)){
      auto metadata=getMetadata(url,fetch,signal);
      if(!metadata.is_null())return metadata;
    }
  return Json();
}
Discovery discoverOAuthServerInfo(const std::string& url,const Json& config,
    const std::optional<std::string>& resourceUrl,const http::Fetch& fetch,
    std::stop_token signal){
  Discovery result;result.resource=url;
  const auto configured=field(field(config,"oauth"),"authServerMetadataUrl");
  if(configured.is_string()){
    const auto location=configured.get<std::string>();
    if(!location.starts_with("https://"))
      throw std::runtime_error("authServerMetadataUrl must use https:// (got: "+location+")");
    result.authorizationServerMetadata=getMetadata(location,fetch,signal);
    if(result.authorizationServerMetadata.is_null())
      throw std::runtime_error("Configured OAuth authorization metadata was not found");
    result.authorizationServerUrl=text(field(result.authorizationServerMetadata,"issuer"));
    return result;
  }
  auto candidates=metadataLocations(url,"oauth-protected-resource");
  if(resourceUrl)candidates.insert(candidates.begin(),*resourceUrl);
  std::set<std::string> seen;
  for(const auto& location:candidates){
    if(!seen.insert(location).second)continue;
    auto metadata=getMetadata(location,fetch,signal);
    if(metadata.is_null())continue;
    const auto& servers=field(metadata,"authorization_servers");
    if(!servers.is_array()||servers.empty())continue;
    result.resourceMetadataUrl=location;result.resourceMetadata=metadata;
    const auto resource=text(field(metadata,"resource"));
    if(!resource.empty())result.resource=resource;
    result.authorizationServerUrl=servers.front().get<std::string>();
    result.authorizationServerMetadata=discoverAuthorizationServerMetadata(
      result.authorizationServerUrl,fetch,signal);
    return result;
  }
  result.authorizationServerUrl=origin(url);
  result.authorizationServerMetadata=discoverAuthorizationServerMetadata(url,fetch,signal);
  if(const auto issuer=text(field(result.authorizationServerMetadata,"issuer"));!issuer.empty())
    result.authorizationServerUrl=issuer;
  return result;
}
Json fetchAuthServerMetadata(const std::string& url,const Json& config,
    const std::optional<std::string>& resource,const http::Fetch& fetch,std::stop_token signal){
  if(field(field(config,"oauth"),"authServerMetadataUrl").is_string())
    return discoverOAuthServerInfo(url,config,resource,fetch,signal).authorizationServerMetadata;
  try{
    const auto state=discoverOAuthServerInfo(url,config,resource,fetch,signal);
    if(!state.authorizationServerMetadata.is_null())return state.authorizationServerMetadata;
  }catch(const std::exception&){if(signal.stop_requested())throw;}
  const auto parsed=runtime::parseUrl(url);
  if(!parsed||parsed->at("pathname")=="/")return Json();
  return discoverAuthorizationServerMetadata(url,fetch,signal);
}
Json exchangeToken(const std::string& endpoint,Json values,const Json& client,
    const http::Fetch& fetch,std::stop_token signal,const Json& metadata){
  http::Request request;request.method="POST";request.url=endpoint;
  request.headers={{"Content-Type","application/x-www-form-urlencoded"},{"Accept","application/json"}};
  const auto methods=field(metadata,"token_endpoint_auth_methods_supported");
  const bool hasSecret=field(client,"client_secret").is_string();
  const auto supported=[&](const std::string& name){
    if(!methods.is_array()||methods.empty())return true;
    return std::find(methods.begin(),methods.end(),Json(name))!=methods.end();
  };
  std::string method=text(field(client,"token_endpoint_auth_method"));
  if((method!="none"&&method!="client_secret_basic"&&method!="client_secret_post")||!supported(method))
    method=hasSecret&&supported("client_secret_basic")?"client_secret_basic":
      hasSecret&&supported("client_secret_post")?"client_secret_post":supported("none")?"none":
      hasSecret?"client_secret_post":"none";
  if(method=="client_secret_basic"){
    const auto escaped=[](const Json& value){
      const auto encoded=http::form(Json{{"value",value}});return encoded.substr(6);
    };
    const auto credentials=escaped(client.at("client_id"))+':'+escaped(client.at("client_secret"));
    std::string encoded(4*((credentials.size()+2)/3),'\0');
    const auto size=EVP_EncodeBlock(reinterpret_cast<unsigned char*>(encoded.data()),
      reinterpret_cast<const unsigned char*>(credentials.data()),static_cast<int>(credentials.size()));
    if(size<0)throw std::runtime_error("OAuth client authentication encoding failed");
    encoded.resize(static_cast<std::size_t>(size));request.headers["Authorization"]="Basic "+encoded;
  }else{
    values["client_id"]=client.at("client_id");
    if(method=="client_secret_post")values["client_secret"]=client.at("client_secret");
  }
  request.body=http::form(values);request.signal=signal;
  auto response=normalizeOAuthErrorBody(fetch(request));
  auto body=Json::parse(response.body,nullptr,false);
  if(!response.ok()){
    auto code=text(field(body,"error"));
    if(code.empty())code=response.status==429?"too_many_requests":response.status>=500?"server_error":"request_failed";
    const auto message=text(field(body,"error_description"));
    throw OAuthError(code,message.empty()?"OAuth token request failed with HTTP "+
      std::to_string(response.status):message,response.status);
  }
  const auto tokens=parseOAuthTokens(body);
  if(!tokens)
    throw std::runtime_error("OAuth token response is missing access_token or token_type");
  return *tokens;
}
} // namespace batbox::mcp::auth
