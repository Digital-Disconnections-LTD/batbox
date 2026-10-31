#include "cli/structured/RequestProducer.hpp"
#include "runtime/JsonValue.hpp"
#include <memory>
#include <stdexcept>
#include <stop_token>
#include <thread>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  auto outbound=std::make_shared<cli::structured::AsyncStream<Json>>(loop);
  auto control=std::make_shared<cli::structured::ControlResponses>(
    [](const Json&){});
  cli::structured::RequestProducer producer(loop,control,outbound,
    {[]{return std::string("request-1");},{}});
  std::stop_source stop;
  auto pending=producer.sendRequest(Json{{"subtype","can_use_tool"},
    {"tool_use_id","tool-1"}},[](const Json& value){return value;},
    stop.get_token());
  std::thread worker([&]{stop.request_stop();});
  worker.join();
  std::exception_ptr error;
  pending.then([&](const Json*,const auto& failure){error=failure;});
  loop.runReady();
  if(!error)throw std::runtime_error("Abort did not reject request");
  try{std::rethrow_exception(error);}
  catch(const cli::structured::RequestAbortError&){}
  catch(...){throw std::runtime_error("Wrong abort error type");}
  if(control->hasPendingRequest("request-1")||
     !control->hasResolvedGadgetUseId("tool-1"))
    throw std::runtime_error("Abort bookkeeping mismatch");
  const auto queued=outbound->snapshotQueued();
  if(queued.size()!=2||queued.at(1)!=Json{{"type","control_cancel_request"},
    {"request_id","request-1"}})
    throw std::runtime_error("Abort cancellation not enqueued");
  auto unresolved=producer.sendRequest(Json{{"subtype","mcp_message"}},
    [](const Json& value){return value;},{},"request-2");
  producer.closeInput();
  std::exception_ptr closedError;
  unresolved.then([&](const Json*,const auto& failure){closedError=failure;});
  loop.runReady();
  if(!closedError||control->hasPendingRequest("request-2"))
    throw std::runtime_error("Input close did not reject and clean request");
}
