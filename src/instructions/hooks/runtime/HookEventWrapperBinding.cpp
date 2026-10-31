#include "instructions/hooks/runtime/HookEventWrappers.hpp"

namespace batbox::instructions::hooks::runtime {
HookEventWrapperServices bindHookEventWrapperServices(event_loop::Loop& loop,
  BaseHookInputServices base,std::function<std::string()> randomUUID,
  ExecuteHooksServices executor){
  HookEventWrapperServices services;
  services.createBaseHookInput=[base=std::move(base)](
    std::optional<std::string> mode,std::optional<std::string> flight,
    std::optional<HookBatInfo> bat){
    return createBaseHookInput(base,std::move(mode),std::move(flight),
      std::move(bat)).toJson();
  };
  services.randomUUID=std::move(randomUUID);
  services.executeHooks=[&loop,executor=std::move(executor)](
    ExecuteHooksInput input){
    auto generator=std::make_shared<ExecuteHooks>(loop,std::move(input),executor);
    return HookYieldCursor{
      [generator]{return generator->next();},
      [generator](std::optional<Json> value){
        return generator->returnValue(std::move(value));},
      [generator](std::exception_ptr error){
        return generator->throwError(error);}};
  };
  return services;
}
} // namespace batbox::instructions::hooks::runtime
