#include "model/LiveStreamAdapter.hpp"
#include "model/LiveStream.hpp"
#include "model/StreamObservation.hpp"

namespace batbox::model {

OpenedStream openLiveStream(transport::Client& client, const Json& streamingParams,
                            std::stop_token signal,
                            const std::optional<std::string>&) {
  // Retain server-provided request IDs without adding a correlation header.
  auto stream = std::make_shared<LiveStream>(client, streamingParams, signal);
  auto response = stream->open();
  OpenedStream opened;
  opened.response = response;
  const auto& requestId = value::property(response.headers, "request-id");
  if (requestId.is_string()) opened.requestId = requestId.get<std::string>();
  opened.next = [stream] { return stream->next(); };
  opened.abort = [stream] { stream->abort(); };
  return opened;
}

void consumeLiveStream(OpenedStream& opened, streaming::Reducer& reducer,
                       const std::function<void(const streaming::MessageRef&)>& yield,
                       StreamWatchdog* watchdog,
                       const std::function<void()>& onFirstChunk,
                       StreamObservation* observation) {
  try {
    bool isFirstChunk=true;
    while (auto event = opened.next()) {
      if (watchdog) watchdog->reset();
      if(observation)observation->onEvent();
      if(isFirstChunk){
        if(onFirstChunk)onFirstChunk();
        isFirstChunk=false;
      }
      for (const auto& emission : reducer.consume(*event)) yield(emission);
    }
    if (watchdog) {
      watchdog->clear();
      if (watchdog->aborted()){
        if(observation)observation->watchdogCleanExit();
        throw std::runtime_error("Stream idle timeout - no chunks received");
      }
    }
    if(observation)observation->beforeFinish(reducer);
    reducer.finish();
  } catch (...) {
    if (watchdog) watchdog->clear();
    if (opened.abort) opened.abort();
    throw;
  }
  if (opened.abort) opened.abort();
}

} // namespace batbox::model
