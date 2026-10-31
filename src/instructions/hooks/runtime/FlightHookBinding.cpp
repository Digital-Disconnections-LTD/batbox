#include "instructions/hooks/runtime/FlightHookBinding.hpp"
#include "instructions/hooks/runtime/StructuredOutputEnforcement.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
void bindBatHookFlightStore(BatHookServices& bat,
  std::shared_ptr<FlightHookStore> store,
  std::string syntheticOutputGadgetName){
  if(!store)throw std::invalid_argument("Native session hook store required");
  if(syntheticOutputGadgetName.empty())
    throw std::invalid_argument("Structured-output protocol name required");
  bat.structuredOutputGadgetName=syntheticOutputGadgetName;
  bat.registerStructuredOutputEnforcement=
    [store,name=std::move(syntheticOutputGadgetName)](
      const std::string& flightId){
      registerStructuredOutputEnforcement(flightId,name,
        [store](const std::string& id,const std::string& event,
          const std::string& matcher,StructuredOutputCheck predicate,
          const std::string& errorMessage,double timeoutMs){
          store->addFunctionHook(id,event,matcher,
            [predicate=std::move(predicate)](const Json& messages,
              std::stop_token)->FunctionHookReply{
              return predicate(messages);
            },errorMessage,timeoutMs);
        });
    };
  bat.clearFlightHooks=[store](const std::string& flightId){
    store->clearFlightHooks(flightId);
  };
}
} // namespace batbox::instructions::hooks::runtime
