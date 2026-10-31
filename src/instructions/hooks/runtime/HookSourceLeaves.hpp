#pragma once
#include "instructions/hooks/runtime/HookBatchStart.hpp"
#include "instructions/hooks/runtime/OutsideRepl.hpp"

namespace batbox::instructions::hooks::runtime {
bool isInternalHook(const HookBatchMatch& matched);
bool hasBlockingResult(const std::vector<HookResult>& results);
void bindHookSourceLeaves(HookBatchStartServices& services);
} // namespace batbox::instructions::hooks::runtime
