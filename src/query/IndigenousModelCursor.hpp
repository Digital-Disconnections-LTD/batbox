#pragma once
#include "query/AsyncModelPass.hpp"
#include "model/QueryModel.hpp"

namespace batbox::query {
using ModelProducer=std::function<void(
  const std::function<void(const model::MessageRef&)>& yield,
  std::stop_token stop)>;

// Indigenous adapter for the synchronous callback producer. Each yielded ref waits
// for the next cursor demand before producer work resumes. Worker completion,
// failure and cancellation settle Deferred values on the caller-owned Loop.
AsyncModelCursor makeProducerModelCursor(event_loop::Loop& loop,
  ModelProducer producer,std::stop_token externalStop={},
  std::function<bool(std::exception_ptr)> expectedCloseError={});

// Concrete binding to the existing indigenous queryModel implementation. Build
// input/services on the owner thread; imported QueryServices callbacks that
// touch application state must themselves be owner-dispatched or thread safe.
AsyncModelCursor makeIndigenousQueryModelCursor(event_loop::Loop& loop,
  model::PreflightInputs input,model::QueryServices services,
  const std::function<void()>& onStreamingFallback);
} // namespace batbox::query
