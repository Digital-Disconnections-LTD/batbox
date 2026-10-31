#include "query/IndigenousModelCursor.hpp"
#include "query/CloseableIndigenousCursor.hpp"

namespace batbox::query {
AsyncModelCursor makeProducerModelCursor(event_loop::Loop& loop,
  ModelProducer producer,std::stop_token externalStop,
  std::function<bool(std::exception_ptr)> expectedCloseError){
  auto indigenous=makeCloseableIndigenousCursor<model::MessageRef>(loop,
    [producer=std::move(producer)](
      const std::function<void(const model::MessageRef&)>& yield,
      std::stop_token stop){
      producer([&](const model::MessageRef& value){
        if(!value)throw std::invalid_argument("Model producer emitted null MessageRef");
        yield(value);
      },stop);
    },externalStop,std::move(expectedCloseError));
  return AsyncModelCursor{[indigenous]{return indigenous.next();},
    [indigenous]{return indigenous.close();}};
}
AsyncModelCursor makeIndigenousQueryModelCursor(event_loop::Loop& loop,
  model::PreflightInputs input,model::QueryServices services,
  const std::function<void()>& onStreamingFallback){
  const std::stop_token external=services.signal;
  const auto existingFallback=services.onStreamingFallback;
  services.onStreamingFallback=[existingFallback,onStreamingFallback]{
    if(existingFallback)existingFallback();
    if(onStreamingFallback)onStreamingFallback();
  };
  return makeProducerModelCursor(loop,
    [input=std::move(input),services=std::move(services)](
      const std::function<void(const model::MessageRef&)>& yield,
      std::stop_token stop) mutable {
      services.signal=stop;
      (void)model::queryModel(input,std::move(services),yield);
    },external);
}
} // namespace batbox::query
