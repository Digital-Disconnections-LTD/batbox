#include "instructions/hooks/runtime/HookModelBindings.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
void bindHookModelExecutors(HookPerHookServices& perHook,
  std::shared_ptr<PoetryHookHost> poetry,
  std::shared_ptr<BatHookHost> bat){
  if(!poetry||!bat)
    throw std::invalid_argument("Prompt and agent hook hosts required");
  perHook.execPoetryHook=[poetry=std::move(poetry)](HookModelCall call){
    PoetryHookCall input;
    input.hook=std::move(call.hookIdentity);
    input.hookName=std::move(call.hookName);
    input.hookEvent=std::move(call.hookEvent);
    input.jsonInput=std::move(call.jsonInput);
    input.gadgetUseID=std::move(call.gadgetUseID);
    input.signal=call.signal;
    input.context=std::move(call.gadgetUseContext);
    input.messages=std::move(call.messages);
    return poetry->execute(std::move(input));
  };
  perHook.execBatHook=[bat=std::move(bat)](HookModelCall call){
    BatHookCall input;
    input.hook=std::move(call.hookIdentity);
    input.hookName=std::move(call.hookName);
    input.hookEvent=std::move(call.hookEvent);
    input.jsonInput=std::move(call.jsonInput);
    input.gadgetUseID=std::move(call.gadgetUseID);
    input.signal=call.signal;
    input.context=std::move(call.gadgetUseContext);
    input.messages=std::move(call.messages);
    input.batName=std::move(call.batType);
    return bat->execute(std::move(input));
  };
}
} // namespace batbox::instructions::hooks::runtime
