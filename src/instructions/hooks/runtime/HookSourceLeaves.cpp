#include "instructions/hooks/runtime/HookSourceLeaves.hpp"

namespace batbox::instructions::hooks::runtime {
bool isInternalHook(const HookBatchMatch& matched){
  return matched.hook&&
    value::property(matched.hook->fields,"type")=="callback"&&
    value::property(matched.hook->fields,"internal")==true;
}
bool hasBlockingResult(const std::vector<HookResult>& results){
  for(const auto& result:results)if(result.blocked)return true;
  return false;
}
void bindHookSourceLeaves(HookBatchStartServices& services){
  services.isInternalHook=isInternalHook;
}
} // namespace batbox::instructions::hooks::runtime
