#include "settings/remote/Eligibility.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::settings::remote {
namespace {
bool truthyProperty(const Json& object,const char* key){
  return object.is_object()&&object.contains(key)&&value::truthy(object[key]);
}
bool scopeIncludes(const Json& tokens,const std::string& expected){
  if(!tokens.is_object()||!tokens.contains("scopes"))return false;
  const auto& scopes=tokens["scopes"];
  if(scopes.is_array()){
    for(const auto& scope:scopes)if(scope==expected)return true;
  }else if(scopes.is_string())
    return scopes.get_ref<const std::string&>().find(expected)!=
      std::string::npos;
  return false;
}
}
Eligibility::Eligibility(SyncStash& stash,EligibilityServices services):
  stash_(stash),services_(std::move(services)){
  if(!services_.provider||!services_.isPrimaryBaseUrl||
      !services_.entrypoint||!services_.oauthTokens||
      !services_.apiKeyWithoutHelper)
    throw std::invalid_argument("Missing remote eligibility binding");
}
bool Eligibility::isRemoteManagedSettingsEligible(){
  if(stashed_)return *stashed_;
  const auto finish=[this](bool value){
    stashed_=stash_.setEligibility(value);return value;
  };
  if(services_.provider()!=services_.primaryProvider)return finish(false);
  if(!services_.isPrimaryBaseUrl())return finish(false);
  if(services_.entrypoint()==services_.localBatEntrypoint)
    return finish(false);
  const auto tokens=services_.oauthTokens();
  if(truthyProperty(tokens,"accessToken")&&
      tokens.contains("subscriptionType")&&
      tokens["subscriptionType"].is_null())return finish(true);
  if(truthyProperty(tokens,"accessToken")&&
      scopeIncludes(tokens,services_.inferenceScope)&&
      tokens.contains("subscriptionType")&&
      (tokens["subscriptionType"]=="enterprise"||
       tokens["subscriptionType"]=="team"))return finish(true);
  try{
    const auto key=services_.apiKeyWithoutHelper();
    if(key&&!key->empty())return finish(true);
  }catch(...){}
  return finish(false);
}
void Eligibility::resetSyncStash(){stashed_.reset();stash_.resetSyncStash();}
}
