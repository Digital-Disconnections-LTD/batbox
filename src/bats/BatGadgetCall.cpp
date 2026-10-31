#include "bats/BatGadgetCall.hpp"

namespace batbox::bats {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
}
Json callBatGadget(const Json& input,const BatContext& context,
    const std::function<std::future<Json>(const Json&)>& canUseGadget,
    const Json& assistantMessage,BatGadgetCallServices& services){
  const Json& options=field(context.query.data,"options");
  const Json& messages=field(context.query.data,"messages");
  // BatGadget.call reads gadgetUseContext.getAppState, including any live
  // wrapper installed by an earlier gadget result in this query turn.
  services.preflight.getAppState=effectiveBatGetAppState(context);
  auto preflight=preflightBatGadget(input,options,messages,assistantMessage,
    services.preflight);
  if(preflight.immediateResult)return *preflight.immediateResult;
  auto plan=prepareBatGadgetLaunch(input,preflight,assistantMessage,context,
    canUseGadget,services.launch);
  if(plan.shouldRunAsync)
    return launchBatGadgetAsync(input,assistantMessage,context,std::move(plan),
      services.async);
  return runBatGadgetForeground(input,assistantMessage,context,std::move(plan),
    services.foreground);
}
} // namespace batbox::bats
