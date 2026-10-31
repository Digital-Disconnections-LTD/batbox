#include "instructions/hooks/runtime/PoetryGadgetWrappers.hpp"

namespace batbox::instructions::hooks::runtime {
PoetryGadgetServices bindPoetryGadgetServices(event_loop::Loop& loop,
  HookEventWrapperServices wrapper,std::function<std::string()> getFlightId,
  std::function<bool(const std::string&,const std::optional<Json>&,
    const std::string&)> hasHookForEvent,
  std::function<void(const std::string&,const std::optional<Json>&)>
    logForDebugging,ExecuteHooksServices executor){
  PoetryGadgetServices services{std::move(wrapper),std::move(getFlightId),
    std::move(hasHookForEvent),std::move(logForDebugging),{}};
  services.executeWithPoetry=[&loop,executor=std::move(executor)](
    ExecuteHooksInput call,RequestPoetryFactory poetry){
    auto bound=executor;
    bound.batch.bindRequestPoetry=std::move(poetry);
    auto generator=std::make_shared<ExecuteHooks>(loop,std::move(call),
      std::move(bound));
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
