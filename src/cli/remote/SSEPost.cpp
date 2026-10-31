#include "cli/remote/SSETransport.hpp"
#include <algorithm>
#include <cmath>

namespace batbox::cli::remote {
Deferred<Unit> SSETransport::write(const Json& message){
  Deferred<Unit> result(loop_);
  try{
    const auto auth=services_.getAuthHeaders();
    if(!auth.is_object()||auth.empty()){
      services_.logDebug("SSETransport: No session token available for POST",
        nullptr);
      result.resolve({});return result;
    }
    Json headers=auth;
    headers["Content-Type"]="application/json";
    headers[services_.protocolVersionHeaderName]=
      services_.protocolVersionHeaderValue;
    headers["User-Agent"]=services_.userBat();
    postAttempt(message,std::move(headers),1,result);
  }catch(...){result.reject(std::current_exception());}
  return result;
}
void SSETransport::postAttempt(Json message,Json headers,int attempt,
    const Deferred<Unit>& result){
  try{
    auto request=services_.post(postUrl_,headers,message);
    const std::weak_ptr<SSETransport> weak=shared_from_this();
    request.then([weak,message=std::move(message),headers=std::move(headers),
      attempt,result](const SseHttpResponse* response,
      const std::exception_ptr& error){
      const auto self=weak.lock();
      if(!self){result.resolve({});return;}
      if(!error&&response){
        if(response->status==200||response->status==201){
          result.resolve({});return;
        }
        if(response->status>=400&&response->status<500&&
           response->status!=429){
          result.resolve({});return;
        }
      }
      if(attempt==10){result.resolve({});return;}
      const auto delay=std::min(500.0*std::pow(2.0,attempt-1),8000.0);
      self->loop_.setTimeout(delay,[weak,message,headers,attempt,result]{
        if(const auto current=weak.lock())
          current->postAttempt(message,headers,attempt+1,result);
        else result.resolve({});
      });
    });
  }catch(...){
    if(attempt==10){result.resolve({});return;}
    const auto delay=std::min(500.0*std::pow(2.0,attempt-1),8000.0);
    const std::weak_ptr<SSETransport> weak=shared_from_this();
    loop_.setTimeout(delay,[weak,message=std::move(message),
      headers=std::move(headers),attempt,result]{
      if(const auto current=weak.lock())
        current->postAttempt(message,headers,attempt+1,result);
      else result.resolve({});
    });
  }
}
}
