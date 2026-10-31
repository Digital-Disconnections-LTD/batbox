#pragma once

#include "model/StreamingAttempt.hpp"
#include "streaming/Reducer.hpp"
#include "model/Watchdog.hpp"

namespace batbox::model {
class StreamObservation;

OpenedStream openLiveStream(transport::Client& client, const Json& streamingParams,
                            std::stop_token signal,
                            const std::optional<std::string>& clientRequestId = std::nullopt);
void consumeLiveStream(OpenedStream& opened, streaming::Reducer& reducer,
                       const std::function<void(const streaming::MessageRef&)>& yield,
                       StreamWatchdog* watchdog = nullptr,
                       const std::function<void()>& onFirstChunk = {},
                       StreamObservation* observation=nullptr);

} // namespace batbox::model
