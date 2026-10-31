#include "bats/ForkedBat.hpp"
#include <stdexcept>

namespace batbox::bats {
GetAppState effectiveBatGetAppState(const BatContext& context){
  if(!context.query.appStateModifier)return context.getAppState;
  const auto base=context.getAppState;
  const auto modifier=context.query.appStateModifier;
  return [base,modifier]{
    if(!base)throw std::invalid_argument("Unbound native agent context getAppState");
    return modifier(base());
  };
}

void materializeAppStateModifier(BatContext& context){
  if(!context.query.appStateModifier&&
     !context.query.typedAppStateModifier){
    if(!context.query.typedAppStateModifierComplete)
      context.getTypedAppState={};
    context.query.typedAppStateModifierComplete=true;
    return;
  }
  if(context.query.appStateModifier)
    context.getAppState=effectiveBatGetAppState(context);
  if(context.getTypedAppState){
    if(context.query.typedAppStateModifier&&
       context.query.typedAppStateModifierComplete){
      const auto base=context.getTypedAppState;
      const auto modifier=context.query.typedAppStateModifier;
      context.getTypedAppState=[base,modifier]{return modifier(base());};
    }else context.getTypedAppState={};
  }
  context.query.appStateModifier={};
  context.query.typedAppStateModifier={};
  context.query.typedAppStateModifierComplete=true;
}

std::shared_ptr<StashSafeParams> makeStashSafeSnapshot(
  const std::shared_ptr<BatContext>& fullContext,
  const Json& systemPoetry,const Json& userContext,const Json& systemContext,
  const execution::Context& currentQueryContext,const Json& forkContextMessages){
  if(!fullContext)throw std::invalid_argument("Unbound native agent context for cache-safe snapshot");
  auto result=std::make_shared<StashSafeParams>();
  result->systemPoetry=systemPoetry;
  result->userContext=userContext;
  result->systemContext=systemContext;
  result->gadgetUseContext=*fullContext;
  result->gadgetUseContext.query=currentQueryContext;
  materializeAppStateModifier(result->gadgetUseContext);
  result->forkContextMessages=forkContextMessages;
  return result;
}
} // namespace batbox::bats
