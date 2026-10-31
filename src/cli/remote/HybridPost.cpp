#include "cli/remote/HybridTransport.hpp"
#include <stdexcept>

namespace batbox::cli::remote {
Deferred<Unit> HybridTransport::postOnce(const std::vector<Json>& events){
  Deferred<Unit> result(loop_);
  const auto token=services_.getFlightIngressAuthToken();
  if(!token||token->empty()){
    services_.logDebug("HybridTransport: No session token available for POST",
      nullptr);
    result.resolve({});return result;
  }
  Json headers={{"Authorization","Bearer "+*token},
    {"Content-Type","application/json"}};
  Json body={{"events",events}};
  try{
    auto request=services_.post(postUrl_,headers,body);
    const std::weak_ptr<HybridTransport> weak=weak_from_this();
    request.then([result,weak,count=events.size()](const SseHttpResponse* response,
      const std::exception_ptr& error){
      if(error){result.reject(error);return;}
      if(!response){
        result.reject(std::make_exception_ptr(std::runtime_error(
          "Hybrid POST response missing")));return;
      }
      const int status=response->status;
      if(status>=200&&status<300){
        if(const auto self=weak.lock())self->services_.logDebug(
          "HybridTransport: POST success count="+std::to_string(count),nullptr);
        result.resolve({});return;
      }
      if(status>=400&&status<500&&status!=429){
        if(const auto self=weak.lock())self->services_.logDebug(
          "HybridTransport: POST returned "+std::to_string(status)+
            " (permanent), dropping",nullptr);
        result.resolve({});return;
      }
      if(const auto self=weak.lock())self->services_.logDebug(
        "HybridTransport: POST returned "+std::to_string(status)+
          " (retryable)",nullptr);
      result.reject(std::make_exception_ptr(std::runtime_error(
        "POST failed with "+std::to_string(status))));
    });
  }catch(...){result.reject(std::current_exception());}
  return result;
}
}
