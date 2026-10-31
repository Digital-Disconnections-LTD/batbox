#include "auth/OAuthTokens.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::auth {
namespace {
std::string errorMessage(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}
  catch(const std::exception& caught){return caught.what();}
  catch(...){return "unknown error";}
}
}
OAuthTokens::OAuthTokens(event_loop::Loop& loop,OAuthTokenServices services):
  loop_(loop),services_(std::move(services)){
  if(!services_.isBareMode||!services_.environment||
      !services_.oauthTokenFromFd||!services_.secureStorageRead||
      !services_.secureStorageReadAsync||!services_.logError||
      !services_.clearKeychainStash)
    throw std::invalid_argument("Missing OAuth token binding");
}
std::optional<Json> OAuthTokens::readStored(
    const std::optional<Json>& storage)const{
  if(!storage||!storage->is_object()||
      !storage->contains(services_.storageKey))return std::nullopt;
  const auto& tokens=(*storage)[services_.storageKey];
  if(!tokens.is_object()||!tokens.contains("accessToken")||
      !value::truthy(tokens["accessToken"]))return std::nullopt;
  return tokens;
}
std::optional<Json> OAuthTokens::getBatboxOAuthTokens(){
  if(memoized_)return *memoized_;
  if(services_.isBareMode()){
    memoized_=std::optional<Json>{};return std::nullopt;
  }
  const auto envToken=services_.environment(services_.oauthEnv);
  if(envToken&&!envToken->empty()){
    const auto token=services_.environment(services_.oauthEnv);
    memoized_=Json{{"accessToken",token?*token:std::string{}},
      {"refreshToken",nullptr},
      {"expiresAt",nullptr},{"scopes",Json::array({services_.inferenceScope})},
      {"subscriptionType",nullptr},{"rateLimitTier",nullptr}};
    return *memoized_;
  }
  const auto fdToken=services_.oauthTokenFromFd();
  if(fdToken&&!fdToken->empty()){
    memoized_=Json{{"accessToken",*fdToken},{"refreshToken",nullptr},
      {"expiresAt",nullptr},{"scopes",Json::array({services_.inferenceScope})},
      {"subscriptionType",nullptr},{"rateLimitTier",nullptr}};
    return *memoized_;
  }
  try{
    memoized_=readStored(services_.secureStorageRead());
    return *memoized_;
  }catch(...){
    services_.logError(errorMessage(std::current_exception()));
    memoized_=std::optional<Json>{};return std::nullopt;
  }
}
event_loop::Deferred<std::optional<Json>>
OAuthTokens::getBatboxOAuthTokensAsync(){
  event_loop::Deferred<std::optional<Json>> output(loop_);
  try{
    if(services_.isBareMode()){output.resolve(std::nullopt);return output;}
    const auto envToken=services_.environment(services_.oauthEnv);
    const auto fdToken=envToken&&!envToken->empty()
      ?std::optional<std::string>{}:services_.oauthTokenFromFd();
    if((envToken&&!envToken->empty())||(fdToken&&!fdToken->empty())){
      output.resolve(getBatboxOAuthTokens());return output;
    }
    services_.secureStorageReadAsync().then([this,output](
        const std::optional<Json>* storage,const std::exception_ptr& error){
      if(error){
        try{services_.logError(errorMessage(error));output.resolve(std::nullopt);}
        catch(...){output.reject(std::current_exception());}
        return;
      }
      try{output.resolve(readStored(*storage));}
      catch(...){
        try{services_.logError(errorMessage(std::current_exception()));
          output.resolve(std::nullopt);}
        catch(...){output.reject(std::current_exception());}
      }
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
void OAuthTokens::clearOAuthTokenStash(){
  memoized_.reset();services_.clearKeychainStash();
}
}
