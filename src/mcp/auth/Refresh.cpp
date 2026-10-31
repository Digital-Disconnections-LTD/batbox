#include "mcp/auth/Provider.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <system_error>
#include <vector>

namespace batbox::mcp::auth {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string string(const Json& value){return value.is_string()?value.get<std::string>():std::string{};}
std::vector<std::string> scopes(const std::string& value){
  std::vector<std::string> output;std::size_t at=0;
  for(;;){const auto next=value.find(' ',at);output.push_back(value.substr(at,next-at));
    if(next==std::string::npos)break;
    at=next+1;
  }return output;
}
Json wireTokens(const Json& entry,double expires,bool omitRefresh=false){
  Json value={{"access_token",field(entry,"accessToken")},{"expires_in",expires},{"token_type","Bearer"}};
  if(!omitRefresh&&!field(entry,"refreshToken").is_null())value["refresh_token"]=field(entry,"refreshToken");
  if(!field(entry,"scope").is_null())value["scope"]=field(entry,"scope");
  return value;
}
bool transient(const std::exception& error){
  if(const auto* oauth=dynamic_cast<const OAuthError*>(&error))
    if(oauth->code=="server_error"||oauth->code=="temporarily_unavailable"||
        oauth->code=="too_many_requests")return true;
  std::string text=error.what();std::transform(text.begin(),text.end(),text.begin(),
    [](unsigned char value){return static_cast<char>(std::tolower(value));});
  for(const auto* part:{"timeout","timed out","etimedout","econnreset"})
    if(text.find(part)!=std::string::npos)return true;
  return false;
}
}
Json Provider::tokens(){
  // Serialize the refresh state transition at the Indigenous worker boundary so
  // concurrent requests share the persisted result without duplicate refreshes.
  std::lock_guard lock(mutex_);
  const auto record=entry();
  if(record.is_null()||record.is_discarded())return Json();
  const double expires=(value::numberOr(field(record,"expiresAt"))-services_.now())/1000;
  bool needsStepUp=false;
  if(pendingStepUpScope_){
    const auto current=scopes(string(field(record,"scope")));
    for(const auto& wanted:scopes(*pendingStepUpScope_))
      if(std::find(current.begin(),current.end(),wanted)==current.end()){needsStepUp=true;break;}
  }
  if(expires<=0&&!value::truthy(field(record,"refreshToken")))return Json();
  if(expires<=300&&value::truthy(field(record,"refreshToken"))&&!needsStepUp){
    try{
      const auto refreshed=refreshAuthorization(record.at("refreshToken").get<std::string>());
      if(!refreshed.is_null())return refreshed;
    }catch(const std::exception& error){debug(error.what());}
  }
  return wireTokens(record,expires,needsStepUp);
}
Json Provider::refreshAuthorization(const std::string& original){
  std::lock_guard lock(mutex_);
  std::shared_ptr<void> refreshLock;
  if(services_.lockRefresh){
    for(int attempt=0;attempt<5;++attempt){
      try{refreshLock=services_.lockRefresh(getServerKey(name_,config_));break;}
      catch(const OAuthError& error){
        if(error.code!="ELOCKED"){debug("Refresh lock unavailable; proceeding without lock");break;}
        if(!services_.sleep||!services_.random)throw std::invalid_argument("MCP refresh lock retry bindings required");
        services_.sleep(1000+services_.random()*1000);
      }catch(const std::exception&){debug("Refresh lock unavailable; proceeding without lock");break;}
    }
  }
  if(services_.clearStorageStash)services_.clearStorageStash();
  const auto record=entry();std::string token=original;
  if(!record.is_null()&&!record.is_discarded()){
    const double expires=(value::numberOr(field(record,"expiresAt"))-services_.now())/1000;
    if(expires>300)return wireTokens(record,expires);
    if(value::truthy(field(record,"refreshToken")))token=record.at("refreshToken").get<std::string>();
  }
  return doRefresh(token);
}
Json Provider::refreshRejectedAccessToken(){
  std::lock_guard lock(mutex_);
  const auto current=entry();
  const auto& token=field(current,"refreshToken");
  if(!token.is_string()||token.get<std::string>().empty()||pendingStepUpScope_)return Json();
  return doRefresh(token.get<std::string>());
}
Json Provider::doRefresh(const std::string& token){
  if(!services_.discover||!services_.refresh)
    throw std::invalid_argument("MCP OAuth discovery and refresh bindings required");
  for(int attempt=1;attempt<=3;++attempt){
    try{
      auto metadata=metadata_;
      if(metadata.is_null()){
        const auto stashed=discoveryState();
        if(!field(stashed,"authorizationServerMetadata").is_null())
          metadata=stashed.at("authorizationServerMetadata");
        else if(value::truthy(field(stashed,"authorizationServerUrl"))&&services_.discoverAuthorizationServer)
          metadata=services_.discoverAuthorizationServer(stashed.at("authorizationServerUrl").get<std::string>());
      }
      if(metadata.is_null())metadata=services_.discover(config_.at("url").get<std::string>(),config_,{});
      if(metadata.is_null())return Json();
      metadata_=metadata;
      const auto client=clientInformation();if(client.is_null())return Json();
      const auto refreshed=services_.refresh(metadata,client,token,
        config_.at("url").get<std::string>());
      if(!refreshed.is_null())saveTokens(refreshed);
      return refreshed;
    }catch(const OAuthError& error){
      if(error.code=="invalid_grant"){
        if(services_.clearStorageStash)services_.clearStorageStash();
        const auto record=entry();
        if(!record.is_null()&&!record.is_discarded()){
          const auto expires=(value::numberOr(field(record,"expiresAt"))-services_.now())/1000;
          if(expires>300)return wireTokens(record,expires);
        }
        invalidateCredentials("tokens");return Json();
      }
      if(!transient(error)||attempt>=3){debug("Token refresh failed");return Json();}
      if(!services_.sleep)throw std::invalid_argument("MCP refresh retry sleep required");
      services_.sleep(1000*std::pow(2,attempt-1));
    }catch(const std::exception& error){
      if(!transient(error)||attempt>=3){debug("Token refresh failed");return Json();}
      if(!services_.sleep)throw std::invalid_argument("MCP refresh retry sleep required");
      services_.sleep(1000*std::pow(2,attempt-1));
    }
  }
  return Json();
}
} // namespace batbox::mcp::auth
