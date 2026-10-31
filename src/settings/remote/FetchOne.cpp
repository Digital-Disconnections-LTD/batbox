#include "settings/remote/FetchOne.hpp"
#include <stdexcept>

namespace batbox::settings::remote {
namespace {
FetchResult fromError(const std::exception_ptr& error){
  try{if(error)std::rethrow_exception(error);}
  catch(const HttpError& http){
    if(http.status==404)return {true,Json::object(),"",{},false};
    if(http.kind=="auth")return {false,{}, {},
      "Not authorized for remote settings",true};
    if(http.kind=="timeout")return {false,{}, {},
      "Remote settings request timeout",false};
    if(http.kind=="network")return {false,{}, {},
      "Cannot connect to server",false};
    return {false,{},{},http.what(),false};
  }catch(const std::exception& caught){
    return {false,{},{},caught.what(),false};
  }catch(...){return {false,{}, {},"Unknown error",false};}
  return {false,{},{},"Unknown error",false};
}
}
event_loop::Deferred<FetchResult> fetchRemoteManagedSettings(
    event_loop::Loop& loop,const std::optional<std::string>& stashedChecksum,
    const FetchOneServices& services){
  if(!services.refreshOAuth||!services.baseApiUrl||!services.userBat||
      !services.httpGet||!services.validateResponse||
      !services.validateSettings||!services.settingsErrorMessage||
      !services.debug)
    throw std::invalid_argument("Missing remote fetch binding");
  event_loop::Deferred<FetchResult> output(loop);
  try{
    services.refreshOAuth().then([output,stashedChecksum,services](
        const event_loop::Unit*,const std::exception_ptr& refreshError){
      if(refreshError){output.resolve(fromError(refreshError));return;}
      try{
        const auto auth=getRemoteSettingsAuthHeaders(services.auth);
        if(auth.error){output.resolve({false,{},{},
          "Authentication required for remote settings",true});return;}
        const auto endpoint=getRemoteManagedSettingsEndpoint(
          services.baseApiUrl(),services.settingsPath);
        Json headers=auth.headers;
        headers["User-Agent"]=services.userBat();
        if(stashedChecksum&&!stashedChecksum->empty())
          headers["If-None-Match"]="\""+*stashedChecksum+"\"";
        services.httpGet(endpoint,headers,services.timeoutMs).then(
          [output,stashedChecksum,services](const HttpResponse* response,
              const std::exception_ptr& requestError){
          if(requestError){output.resolve(fromError(requestError));return;}
          try{
            if(response->status==304){
              services.debug("Remote settings: Using cached settings (304)");
              output.resolve({true,Json(nullptr),stashedChecksum,{},false});
              return;
            }
            if(response->status==204||response->status==404){
              services.debug("Remote settings: No settings found ("+
                std::to_string(response->status)+")");
              output.resolve({true,Json::object(),{},{},false});return;
            }
            const auto parsed=services.validateResponse(response->data);
            if(!parsed.success){
              services.debug("Remote settings: Invalid response format - "+
                parsed.errorMessage);
              output.resolve({false,{},{},
                "Invalid remote settings format",false});return;
            }
            const auto validation=services.validateSettings(
              parsed.data["settings"]);
            if(!validation.success){
              services.debug("Remote settings: Settings validation failed - "+
                services.settingsErrorMessage(validation));
              output.resolve({false,{},{},
                "Invalid settings structure",false});return;
            }
            services.debug("Remote settings: Fetched successfully");
            output.resolve({true,validation.data,
              parsed.data["checksum"].get<std::string>(),{},false});
          }catch(...){output.resolve(fromError(std::current_exception()));}
        });
      }catch(...){output.resolve(fromError(std::current_exception()));}
    });
  }catch(...){output.resolve(fromError(std::current_exception()));}
  return output;
}
}
