#include "query/AsyncModelInternal.hpp"
#include <algorithm>

namespace batbox::query::async_model {
void Run::afterStream(){
  services.queryCheckpoint("query_api_streaming_end");
  if(services.sync.feature("CACHED_MICROCOMPACT") &&
     value::truthy(prepared->pendingStashEdits)){
    syncAssistantValues();
    const Json& last=result.assistantRefs.empty()?Json():*result.assistantRefs.back();
    const Json& usage=field(field(last,"message"),"usage");
    const Json& cumulative=field(usage,"cache_deleted_input_tokens");
    const Json& baseline=field(prepared->pendingStashEdits,"baselineCacheDeletedTokens");
    // JS subtracts undefined as NaN, so an absent baseline cannot yield a boundary.
    const double deleted=baseline.is_number()?
      std::max(0.0,(cumulative.is_number()?cumulative.get<double>():0.0)-
        baseline.get<double>()):0.0;
    if(deleted>0){
      need(services.sync.createMicrocompactBoundaryMessage,
        "createMicrocompactBoundaryMessage");
      emit(std::make_shared<Json>(services.sync.createMicrocompactBoundaryMessage(
        field(prepared->pendingStashEdits,"trigger"),0,deleted,
        field(prepared->pendingStashEdits,"deletedToolIds"),Json::array())),
        Stage::AfterStashedBoundary);
      return;
    }
  }
  finish();
}
} // namespace batbox::query::async_model
