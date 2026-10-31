#include "mcp/auth/Discovery.hpp"
#include "auth/oauth_client/FormUrl.hpp"
#include "runtime/Url.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <algorithm>
#include <array>
#include <stdexcept>

namespace batbox::mcp::auth {
namespace {
std::string encode(const unsigned char* bytes,std::size_t size){
  std::string result(4*((size+2)/3),'\0');
  const auto length=EVP_EncodeBlock(reinterpret_cast<unsigned char*>(result.data()),
    bytes,static_cast<int>(size));
  if(length<0)throw std::runtime_error("OAuth base64 encoding failed");
  result.resize(static_cast<std::size_t>(length));
  while(!result.empty()&&result.back()=='=')result.pop_back();
  std::replace(result.begin(),result.end(),'+','-');std::replace(result.begin(),result.end(),'/','_');
  return result;
}
const Json& field(const Json& value,const char* name){return value::property(value,name);}
std::string text(const Json& value){return value.is_string()?value.get<std::string>():std::string{};}
std::string query(const std::string& url,const Json& parameters){
  const auto parsed=runtime::parseUrl(url);
  if(!parsed)throw std::runtime_error("Invalid OAuth authorization endpoint");
  auto href=parsed->at("href").get<std::string>();
  const auto question=href.find('?');const auto hash=href.find('#');
  const auto first=std::min(question,hash);
  auto values=question==std::string::npos?::batbox::auth::oauth_client::QueryEntries{}:
    ::batbox::auth::oauth_client::parseFormQuery(std::string_view(href).substr(question+1,
      hash==std::string::npos?std::string::npos:hash-question-1));
  for(auto it=parameters.begin();it!=parameters.end();++it){
    if(it.value().is_null()||it.value().is_discarded())continue;
    std::erase_if(values,[&](const auto& value){return value.first==it.key();});
    values.emplace_back(it.key(),it.value().get<std::string>());
  }
  return href.substr(0,first)+'?'+::batbox::auth::oauth_client::serializeFormQuery(values)+
    (hash==std::string::npos?std::string{}:href.substr(hash));
}
}
std::string randomBase64Url(std::size_t size){
  std::vector<unsigned char> bytes(size);
  if(RAND_bytes(bytes.data(),static_cast<int>(bytes.size()))!=1)
    throw std::runtime_error("OAuth secure randomness unavailable");
  return encode(bytes.data(),bytes.size());
}
std::string sha256Base64Url(const std::string& value){
  std::array<unsigned char,EVP_MAX_MD_SIZE> digest{};unsigned int length=0;
  if(EVP_Digest(value.data(),value.size(),digest.data(),&length,EVP_sha256(),nullptr)!=1)
    throw std::runtime_error("OAuth PKCE digest failed");
  return encode(digest.data(),length);
}
OAuthFlight::OAuthFlight(Provider& provider,http::Fetch fetch,std::stop_token signal):
  provider_(provider),fetch_(std::move(fetch)),signal_(signal){}
std::string OAuthFlight::begin(const std::optional<std::string>& requested,
    const std::optional<std::string>& resourceUrl){
  const auto server=provider_.config().at("url").get<std::string>();
  discovery_=discoverOAuthServerInfo(server,provider_.config(),resourceUrl,fetch_,signal_);
  auto& metadata=discovery_.authorizationServerMetadata;
  if(metadata.is_null())metadata=Json{{"issuer",discovery_.authorizationServerUrl},
    {"authorization_endpoint",http::resolveUrl(discovery_.authorizationServerUrl,"/authorize")},
    {"token_endpoint",http::resolveUrl(discovery_.authorizationServerUrl,"/token")},
    {"registration_endpoint",http::resolveUrl(discovery_.authorizationServerUrl,"/register")}};
  provider_.setMetadata(metadata);
  Json state={{"authorizationServerUrl",discovery_.authorizationServerUrl}};
  if(discovery_.resourceMetadataUrl)state["resourceMetadataUrl"]=*discovery_.resourceMetadataUrl;
  provider_.saveDiscoveryState(state);
  client_=provider_.clientInformation();
  if(client_.is_null()){
    const auto document=provider_.clientMetadataUrl();
    if(document&&value::truthy(field(metadata,"client_id_metadata_document_supported")))
      client_=Json{{"client_id",*document}};
    else{
      const auto endpoint=text(field(metadata,"registration_endpoint"));
      if(endpoint.empty())throw std::runtime_error("OAuth server needs a configured clientId or dynamic client registration");
      http::Request request;request.method="POST";request.url=endpoint;
      request.headers={{"Content-Type","application/json"},{"Accept","application/json"}};
      request.body=value::stringify(provider_.clientMetadata());request.signal=signal_;
      auto response=normalizeOAuthErrorBody(fetch_(request));
      if(!response.ok())throw OAuthError("registration_failed","OAuth client registration failed with HTTP "+
        std::to_string(response.status),response.status);
      client_=response.json();
      if(!field(client_,"client_id").is_string())throw std::runtime_error("OAuth registration returned no client_id");
    }
    provider_.saveClientInformation(client_);
  }
  const auto verifier=randomBase64Url(32);provider_.saveCodeVerifier(verifier);
  Json parameters={{"response_type","code"},{"client_id",client_.at("client_id")},
    {"redirect_uri",provider_.redirectUrl()},{"state",provider_.state()},
    {"code_challenge",sha256Base64Url(verifier)},{"code_challenge_method","S256"}};
  auto scope=requested.value_or(getScopeFromMetadata(metadata));
  if(!scope.empty())parameters["scope"]=scope;
  if(!discovery_.resource.empty())parameters["resource"]=discovery_.resource;
  const auto url=query(metadata.at("authorization_endpoint").get<std::string>(),parameters);
  provider_.redirectToAuthorization(url);return url;
}
void OAuthFlight::finish(const std::string& code){
  if(discovery_.authorizationServerMetadata.is_null()||client_.is_null())
    throw std::logic_error("OAuth authorization must begin before token exchange");
  Json values={{"grant_type","authorization_code"},{"code",code},
    {"redirect_uri",provider_.redirectUrl()},{"code_verifier",provider_.codeVerifier()}};
  if(!discovery_.resource.empty())values["resource"]=discovery_.resource;
  provider_.saveTokens(exchangeToken(discovery_.authorizationServerMetadata.at("token_endpoint").get<std::string>(),
    std::move(values),client_,fetch_,signal_,discovery_.authorizationServerMetadata));
}
} // namespace batbox::mcp::auth
