#pragma once
#include "model/QueryModel.hpp"
#include "model_stash/AsyncDetection.hpp"
#include "quota/Quota.hpp"

namespace batbox::model_stash {

// Concrete query-model imports for the two local state modules. The caller
// owns the long-lived loop, detector, and quota state for the app flight.
void bindQueryDiagnostics(model::QueryServices& query,
    event_loop::Loop& owner,std::shared_ptr<Detector> stash,
    std::shared_ptr<quota::QuotaState> quota,
    DiffWriterServices diffWriter);

} // namespace batbox::model_stash
