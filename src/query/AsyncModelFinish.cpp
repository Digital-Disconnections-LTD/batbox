#include "query/AsyncModelInternal.hpp"

namespace batbox::query::async_model {
void Run::afterStream(){
  services.queryCheckpoint("query_api_streaming_end");
  finish();
}
} // namespace batbox::query::async_model
