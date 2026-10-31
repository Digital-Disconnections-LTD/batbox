#include "execution/GadgetExecution.hpp"
#include <stdexcept>

namespace batbox::execution {
void appendAppStateModifierPair(Context& context,
    const std::function<Json(const Json&)>& jsonNext,
    const std::function<app_state::AppStateRef(
      const app_state::AppStateRef&)>& typedNext){
  if(!jsonNext||!typedNext)
    throw std::invalid_argument("Both app state modifiers required");
  if(static_cast<bool>(context.appStateModifier)!=
     static_cast<bool>(context.typedAppStateModifier))
    context.typedAppStateModifierComplete=false;
  std::function<Json(const Json&)> jsonCombined;
  if(context.appStateModifier){
    auto prior=context.appStateModifier;
    jsonCombined=[prior=std::move(prior),jsonNext](const Json& base){
      return jsonNext(prior(base));
    };
  }else jsonCombined=jsonNext;
  std::function<app_state::AppStateRef(const app_state::AppStateRef&)>
    typedCombined;
  if(context.typedAppStateModifier){
    auto prior=context.typedAppStateModifier;
    typedCombined=[prior=std::move(prior),typedNext](
      const app_state::AppStateRef& base){
      return typedNext(prior(base));
    };
  }else typedCombined=typedNext;
  context.appStateModifier=std::move(jsonCombined);
  context.typedAppStateModifier=std::move(typedCombined);
}
} // namespace batbox::execution
