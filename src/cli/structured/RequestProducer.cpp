#include "cli/structured/RequestProducer.hpp"
#include "event_loop/AsyncContext.hpp"
#include <exception>
#include <memory>
#include <stdexcept>
#include <utility>

namespace batbox::cli::structured {
namespace {
using StopCallback=std::stop_callback<std::function<void()>>;
struct RequestState {
  std::string id;
  std::unique_ptr<StopCallback> stopCallback;
};
}
RequestProducer::RequestProducer(event_loop::Loop& loop,
    std::shared_ptr<ControlResponses> control,
    std::shared_ptr<AsyncStream<Json>> outbound,
    RequestProducerServices services):
  loop_(loop),control_(std::move(control)),outbound_(std::move(outbound)),
  services_(std::move(services)){
  if(!control_||!outbound_)
    throw std::invalid_argument("Request producer state required");
}

event_loop::Deferred<Json> RequestProducer::sendRequest(
    const Json& request,std::function<Json(const Json&)> parseResponse,
    std::stop_token signal,const std::optional<std::string>& requestId){
  event_loop::Deferred<Json> output(loop_);
  try{
    const auto id=requestId?*requestId:services_.randomUUID();
    const Json message={{"type","control_request"},
      {"request_id",id},{"request",request}};
    if(inputClosed_)throw std::runtime_error("Stream closed");
    if(signal.stop_requested())throw std::runtime_error("Request aborted");
    if(!parseResponse)throw std::invalid_argument("Response schema required");
    outbound_->enqueue(message);
    if(value::property(request,"subtype")=="can_use_tool"&&
       services_.onControlRequestSent)
      services_.onControlRequestSent(message);
    auto state=std::make_shared<RequestState>();state->id=id;
    if(signal.stop_possible()){
      const auto dispatcher=loop_.dispatcher();
      const std::weak_ptr<RequestState> weak=state;
      const std::weak_ptr<ControlResponses> weakControl=control_;
      const std::weak_ptr<AsyncStream<Json>> weakOutbound=outbound_;
      auto callback=runtime::bindAsyncContext(
        [weak,dispatcher,weakControl,weakOutbound]{
          auto cancel=[weak,weakControl,weakOutbound]{
            if(const auto state=weak.lock()){
              if(const auto outbound=weakOutbound.lock())
                outbound->enqueue(Json{{"type","control_cancel_request"},
                                       {"request_id",state->id}});
              if(const auto control=weakControl.lock())
                control->abortPendingRequest(state->id,
                  std::make_exception_ptr(RequestAbortError()));
            }
          };
          try{
            if(dispatcher.isOwnerThread())cancel();
            else dispatcher.postMission(std::move(cancel));
          }catch(...){}
        });
      state->stopCallback=std::make_unique<StopCallback>(
        signal,std::function<void()>(std::move(callback)));
    }
    PendingRequest pending;
    pending.request=message;
    pending.parseResponse=std::move(parseResponse);
    pending.resolve=[output](const Json& value){output.resolve(value);};
    pending.reject=[output](const std::string& error){
      output.reject(std::make_exception_ptr(std::runtime_error(error)));
    };
    pending.rejectException=[output](std::exception_ptr error){
      output.reject(error);
    };
    control_->addPendingRequest(id,std::move(pending));
    const std::weak_ptr<ControlResponses> weakControl=control_;
    output.then([state,weakControl](const Json*,const std::exception_ptr&){
      state->stopCallback.reset();
      if(const auto control=weakControl.lock())
        control->erasePendingRequest(state->id);
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
void RequestProducer::closeInput(){
  inputClosed_=true;
  control_->rejectAllPending(
    "Tool permission stream closed before response received");
}
}
