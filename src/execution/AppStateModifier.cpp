#include "execution/GadgetExecution.hpp"

namespace batbox::execution {
Json effectiveAppState(const Context& context,const Json& base){
  return context.appStateModifier?context.appStateModifier(base):base;
}
void appendAppStateModifier(Context& context,
    const std::function<Json(const Json&)>& next){
  if(!next)return;
  context.typedAppStateModifierComplete=false;
  if(!context.appStateModifier){context.appStateModifier=next;return;}
  auto previous=context.appStateModifier;
  context.appStateModifier=[previous=std::move(previous),next](const Json& base){
    return next(previous(base));
  };
}
} // namespace batbox::execution
