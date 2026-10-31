#include "model_stash/ModelBindings.hpp"
#include <stdexcept>

namespace batbox::model_stash {

void bindQueryDiagnostics(model::QueryServices& query,
    event_loop::Loop& owner,std::shared_ptr<Detector> stash,
    std::shared_ptr<quota::QuotaState> quota,
    DiffWriterServices diffWriter){
  if(!stash||!quota)throw std::invalid_argument("Query diagnostic state is required");
  query.preflight.recordPoetryState=[stash](const Json& snapshot){
    stash->recordPoetryState(snapshot);
  };
  query.postStream.checkResponseForStashBreak=
    makeAsyncStashCheck(owner,std::move(stash),std::move(diffWriter));
  query.postStream.extractQuotaStatusFromHeaders=[quota](const Json& headers){
    quota->extractQuotaStatusFromHeaders(headers);
  };
  query.extractQuotaStatusFromError=[quota](const retry::ErrorRecord& error){
    quota->extractQuotaStatusFromError(error);
  };
}

} // namespace batbox::model_stash
