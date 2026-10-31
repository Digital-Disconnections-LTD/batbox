#include "model/StreamingAttempt.hpp"
#include "query/OwnerSegmentInvoker.hpp"
#include <cassert>
#include <chrono>
#include <future>
#include <thread>

using namespace std::chrono_literals;

int main() {
  batbox::event_loop::Loop loop;
  const auto owner=loop.dispatcher();
  std::vector<std::string> calls;
  batbox::event_loop::Loop::TimerHandle heartbeat;
  batbox::model::StreamingAttemptState state;
  batbox::model::StreamingAttemptDependencies imports;
  imports.nowMillis=[&]{
    assert(loop.isOwnerThread()); calls.push_back("clock"); return std::int64_t{100};};
  imports.paramsFromContext=[&](const batbox::retry::RetryContext&){
    assert(loop.isOwnerThread()); calls.push_back("params");
    return batbox::Json{{"model","test"},{"max_tokens",10}};};
  imports.captureAPIRequest=[&](const batbox::Json&){
    assert(loop.isOwnerThread()); calls.push_back("capture");
    heartbeat=loop.setTimeout(5,[&]{calls.push_back("heartbeat");},true);};
  std::promise<void> finished;
  std::thread worker([&]{
    try {
      batbox::retry::RetryContext context;
      auto prepared=batbox::query::invokeOwnerSegment(owner,{},[&]{
        return batbox::model::prepareStreamingAttempt(2,context,state,imports);
      });
      assert(prepared.requestParams.at("stream") == true);
      assert(state.attemptNumber == 2);
      std::this_thread::sleep_for(30ms); // Simulated .withResponse() await.
      batbox::model::OpenedStream response;
      response.requestId="server-1";
      response.response.status=200;
      (void)batbox::query::invokeOwnerSegment(owner,{},[&]{
        calls.push_back("headers");
        return batbox::model::finishStreamingAttempt(state,std::move(response),imports);
      });
      owner.postMission([&]{loop.stop();});
      finished.set_value();
    } catch (...) {
      owner.postMission([&]{loop.stop();});
      finished.set_exception(std::current_exception());
    }
  });
  auto hold=loop.hold();
  loop.run();
  hold.release();
  worker.join();
  finished.get_future().get();
  assert(state.streamRequestId == "server-1");
  assert(state.streamResponse.status == 200);
  assert(calls.size()==5);
  assert(calls[0]=="clock" && calls[1]=="params" && calls[2]=="capture");
  assert(calls[3]=="heartbeat" && calls[4]=="headers");
}
