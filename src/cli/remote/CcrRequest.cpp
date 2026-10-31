#include "cli/remote/CcrRuntime.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>

namespace batbox::cli::remote {
Deferred<CcrRequestResult> CcrRuntime::request(const std::string& method,
    const std::string& path,const Json& body,const std::string& label,
    double timeoutMs){
  Deferred<CcrRequestResult> answer(loop_);
  Json auth;
  try{auth=services_.getAuthHeaders();}
  catch(...){answer.reject(std::current_exception());return answer;}
  if(!auth.is_object()||auth.empty()){
    answer.resolve({});return answer;
  }
  try{
    Json headers=auth;
    headers["Content-Type"]="application/json";
    headers[services_.protocolVersionHeaderName]=
      services_.protocolVersionHeaderValue;
    headers["User-Agent"]=services_.userBat();
    auto requestResult=services_.http(method,flightBaseUrl_+path,body,
      headers,timeoutMs);
    const std::weak_ptr<CcrRuntime> weak=shared_from_this();
    requestResult.then([weak,answer,label](
        const CcrHttpResponse* response,const std::exception_ptr& error){
      const auto self=weak.lock();
      if(!self){answer.resolve({});return;}
      if(error){
        self->services_.logDebug("CCRClient: "+label+" failed",
          Json{{"level","warn"}});
        answer.resolve({});return;
      }
      try{
        if(!response){answer.resolve({});return;}
        if(response->status>=200&&response->status<300){
          self->consecutiveAuthFailures_=0;
          answer.resolve(CcrRequestResult{true,{}});return;
        }
        if(response->status==409)self->services_.onEpochMismatch();
        if(response->status==401||response->status==403){
          const auto token=self->services_.getFlightIngressAuthToken();
          const auto expiry=token?
            self->services_.decodeJwtExpiry(*token):std::nullopt;
          if(expiry&&*expiry*1000<self->services_.nowMs())
            self->services_.onEpochMismatch();
          ++self->consecutiveAuthFailures_;
          if(self->consecutiveAuthFailures_>=10)
            self->services_.onEpochMismatch();
        }
        self->services_.logDebug("CCRClient: "+label+" returned "+
          std::to_string(response->status),Json{{"level","warn"}});
        if(response->status==429){
          const auto& raw=value::property(response->headers,"retry-after");
          if(raw.is_string()){
            const auto seconds=value::parseInt10(raw.get<std::string>());
            if(!std::isnan(seconds)&&seconds>=0){
              answer.resolve(CcrRequestResult{false,seconds*1000});return;
            }
          }
        }
        answer.resolve({});
      }catch(...){
        self->services_.logDebug("CCRClient: "+label+" failed",
          Json{{"level","warn"}});
        answer.resolve({});
      }
    });
  }catch(...){answer.resolve({});}
  return answer;
}
}
