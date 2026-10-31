#include "auth/recovery/Handle401.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::auth::recovery {
Handle401::Handle401(event_loop::Loop& loop,Services services):
    loop_(loop),services_(std::move(services)){
  if(!services_.clearOAuthTokenStash||!services_.readTokensAsync||
      !services_.checkAndRefresh)
    throw std::invalid_argument("Missing OAuth 401 recovery binding");
}
event_loop::Deferred<bool> Handle401::handle(
    const std::string& failedAccessToken){
  if(const auto found=pending_.find(failedAccessToken);
      found!=pending_.end())return found->second;
  event_loop::Deferred<bool> output(loop_);
  pending_.emplace(failedAccessToken,output);
  const auto finish=[this,failedAccessToken,output](bool value){
    pending_.erase(failedAccessToken);output.resolve(value);
  };
  const auto fail=[this,failedAccessToken,output](
      const std::exception_ptr& error){
    pending_.erase(failedAccessToken);output.reject(error);
  };
  try{
    services_.clearOAuthTokenStash();
    services_.readTokensAsync().then([this,failedAccessToken,finish,fail](
        const std::optional<Json>* tokens,const std::exception_ptr& error){
      if(error){fail(error);return;}
      try{
        if(!*tokens||!(*tokens)->is_object()||
            !(*tokens)->contains("refreshToken")||
            !value::truthy((**tokens)["refreshToken"])){
          finish(false);return;
        }
        const Json current=(*tokens)->contains("accessToken")
          ?(**tokens)["accessToken"]:Json(nullptr);
        if(current!=failedAccessToken){
          finish(true);return;
        }
        services_.checkAndRefresh(0,true).then([finish,fail](
            const bool* refreshed,const std::exception_ptr& refreshError){
          if(refreshError)fail(refreshError);
          else finish(*refreshed);
        });
      }catch(...){fail(std::current_exception());}
    });
  }catch(...){fail(std::current_exception());}
  return output;
}
}
