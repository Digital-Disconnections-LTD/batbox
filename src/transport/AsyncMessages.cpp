#include "transport/AsyncMessages.hpp"
#include "transport/Errors.hpp"
#include "runtime/AsyncLaunch.hpp"
#include <stdexcept>

namespace batbox::transport {
namespace {
struct Attempt {
  Response response;
  std::exception_ptr error;
  bool successfulHeaders=false;
};
struct Run:std::enable_shared_from_this<Run> {
  event_loop::Loop& loop;
  std::shared_ptr<Client> client;
  Json body;
  int maximum,remaining;
  std::stop_token signal;
  SdkRetryClock clock;
  SdkRetryAuth auth;
  event_loop::Deferred<Response> output;
  event_loop::Loop::KeepAlive hold;
  event_loop::Loop::TimerHandle timer;
  std::optional<std::stop_callback<std::function<void()>>> abort;
  bool done=false;
  Run(event_loop::Loop& owner,std::shared_ptr<Client> value,Json request,
      int retries,std::stop_token stop,SdkRetryClock time,SdkRetryAuth state):
    loop(owner),client(std::move(value)),body(std::move(request)),
    maximum(retries),remaining(retries),signal(stop),clock(std::move(time)),
    auth(std::move(state)),output(owner),hold(owner.hold()){}
  void fail(std::exception_ptr error){
    if(done)return;
    done=true;timer.cancel();output.reject(error);hold.release();
  }
  void start(){
    auto weak=weak_from_this();const auto dispatcher=loop.dispatcher();
    abort.emplace(signal,[weak,dispatcher]{
      try{dispatcher.postMission([weak]{
        if(auto self=weak.lock())self->fail(std::make_exception_ptr(APIUserAbortError()));
      });}catch(...){}
    });
    attempt();
  }
  void attempt(){
    if(done)return;
    if(signal.stop_requested()){fail(std::make_exception_ptr(APIUserAbortError()));return;}
    event_loop::Deferred<Attempt> pending(loop);
    auto self=shared_from_this();
    pending.then([self](const Attempt* value,const std::exception_ptr& error){
      if(self->done)return;
      if(error){self->fail(error);return;}
      try{self->finishAttempt(*value);}catch(...){self->fail(std::current_exception());}
    });
    try{
      runtime::contextThread([client=client,request=body,signal=signal,pending]{
        Attempt result;
        try{
          result.response=client->request(request,false,{},signal,[&](const Response& response){
            result.successfulHeaders=response.status>=200&&response.status<300;
          });
        }catch(...){result.error=std::current_exception();}
        try{pending.resolve(std::move(result));}catch(...){}
      }).detach();
    }catch(...){fail(std::current_exception());}
  }
  void finishAttempt(const Attempt& result){
    if(signal.stop_requested()){fail(std::make_exception_ptr(APIUserAbortError()));return;}
    if(!result.error){
      done=true;output.resolve(result.response);hold.release();return;
    }
    // SDK body decoding happens after its fetch/HTTP retry region. Preserve
    // that boundary when curl reports a failure after successful headers.
    if(result.successfulHeaders){fail(result.error);return;}
    bool retry=false;
    Json headers=Json::object();
    try{std::rethrow_exception(result.error);}
    catch(const APIUserAbortError&){fail(result.error);return;}
    catch(const APIConnectionError&){retry=true;}
    catch(const APIError& error){
      if(error.status)retry=sdkShouldRetry(*error.status,error.headers,auth);
      headers=error.headers;
    }
    catch(...){fail(result.error);return;}
    if(remaining<=0||!retry){fail(result.error);return;}
    const double delay=sdkRetryDelay(headers,remaining,maximum,clock);
    auto self=shared_from_this();
    timer=loop.setTimeout(delay,[self]{
      --self->remaining;self->attempt();
    });
  }
};
}
event_loop::Deferred<Response> createMessage(event_loop::Loop& loop,
    std::shared_ptr<Client> client,Json body,int maxRetries,std::stop_token signal,
    SdkRetryClock clock,SdkRetryAuth auth){
  if(!loop.isOwnerThread())throw std::logic_error("Message request requires owner event loop");
  if(!client)throw std::invalid_argument("Message client required");
  if(maxRetries<0)throw std::invalid_argument("maxRetries must be a non-negative integer");
  auto run=std::make_shared<Run>(loop,std::move(client),std::move(body),
    maxRetries,signal,std::move(clock),std::move(auth));
  run->start();return run->output;
}
} // namespace batbox::transport
