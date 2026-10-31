#include "application_bats/IndigenousBatApplication.hpp"
#include "bats/RunBat.hpp"

namespace batbox::application_bats {
void IndigenousBatApplication::runForSkill(const bats::RunBatParams& params,
    const query::Yield& yield){
  requireCapacity(params.gadgetUseContext.query.stop);
  try{
    auto preflight=makeRunPreflight();
    auto setup=makeRunSetup();
    auto loop=makeRunLoop();
    const auto bridge=query_;
    loop.runIndigenousAsyncQuery=[bridge](const query::Params& request,
        const bats::BatContext& context,
        const IndigenousQueryBridge::CanUseGadget& canUse,
        const query::Yield& sink){bridge->run(request,context,canUse,sink);};
    bats::runBat(params,preflight,setup,loop,yield);
  }catch(...){releaseCapacity();throw;}
  releaseCapacity();
}
}
