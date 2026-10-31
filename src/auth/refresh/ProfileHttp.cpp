#include "auth/refresh/ProfileHttp.hpp"
#include <stdexcept>

namespace batbox::auth::refresh {
namespace {
std::string errorMessage(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}
  catch(const std::exception& caught){return caught.what();}
  catch(...){return "unknown error";}
}
}
event_loop::Deferred<std::optional<Json>> getRawOAuthProfile(
    event_loop::Loop& loop,const std::string& accessToken,
    const ProfileHttpServices& services){
  if(!services.baseApiUrl||!services.httpGet||!services.logError)
    throw std::invalid_argument("Missing OAuth profile HTTP binding");
  event_loop::Deferred<std::optional<Json>> output(loop);
  std::string endpoint;
  try{endpoint=services.baseApiUrl()+services.profilePath;}
  catch(...){output.reject(std::current_exception());return output;}
  try{
    services.httpGet(endpoint,Json{{"Authorization","Bearer "+accessToken},
      {"Content-Type","application/json"}},10000).then(
      [output,log=services.logError](
          const HttpResponse* response,const std::exception_ptr& error){
        if(error){
          try{log(errorMessage(error));output.resolve(std::nullopt);}
          catch(...){output.reject(std::current_exception());}
          return;
        }
        output.resolve(response->data);
      });
  }catch(...){
    try{services.logError(errorMessage(std::current_exception()));
      output.resolve(std::nullopt);}
    catch(...){output.reject(std::current_exception());}
  }
  return output;
}
}
