#include "application/bats/BatServices.hpp"
#include "bats/BatResultBindings.hpp"
#include "bats/BatMissionProgress.hpp"
#include <stdexcept>

namespace batbox::application::bats {
batbox::bats::BatGadgetCallServices makeBatGadgetServices(BatServicesInputs inputs){
  if(!inputs.query)throw std::invalid_argument("Agent query bridge required");
  auto gadget=std::move(inputs.gadget);
  auto query=std::move(inputs.query);
  inputs.loop.runIndigenousAsyncQuery=[query](const query::Params& params,
      const batbox::bats::BatContext& context,
      const IndigenousQueryBridge::CanUseGadget& canUseGadget,
      const query::Yield& yield){query->run(params,context,canUseGadget,yield);};
  auto run=[preflight=std::move(inputs.preflight),
      setup=std::move(inputs.setup),loop=std::move(inputs.loop)](
      const batbox::bats::RunBatParams& params,const query::Yield& yield)mutable{
    // Each invocation owns a copy of the imported callbacks. RunBat mutates
    // temporary service structs while preparing nested context and cleanup.
    auto currentPreflight=preflight;
    auto currentSetup=setup;
    auto currentLoop=loop;
    batbox::bats::runBat(params,currentPreflight,currentSetup,currentLoop,yield);
  };
  gadget.foreground.runBat=run;
  gadget.async.runBat=run;
  if(!gadget.foreground.progress.getProgressUpdate)
    gadget.foreground.progress.getProgressUpdate=batbox::bats::getProgressUpdate;
  if(!gadget.foreground.final.getProgressUpdate)
    gadget.foreground.final.getProgressUpdate=batbox::bats::getProgressUpdate;
  if(!gadget.foreground.final.result.getLastAssistantMessage)
    gadget.foreground.final.result=batbox::bats::indigenousBatResultImports();
  return gadget;
}
} // namespace batbox::application::bats
