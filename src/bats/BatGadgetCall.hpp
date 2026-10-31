#pragma once
#include "bats/BatGadgetPreflight.hpp"
#include "bats/BatGadgetLaunch.hpp"
#include "bats/BatGadgetAsync.hpp"
#include "bats/BatGadgetForeground.hpp"

namespace batbox::bats {
struct BatGadgetCallServices {
  BatGadgetPreflightServices preflight;
  BatGadgetLaunchServices launch;
  BatGadgetAsyncServices async;
  BatGadgetForegroundServices foreground;
};

// BatGadget.call orchestration: preflight, launch setup, then the requested
// asynchronous or foreground execution. External services remain explicit.
Json callBatGadget(const Json& input,const BatContext& context,
  const std::function<std::future<Json>(const Json&)>& canUseGadget,
  const Json& assistantMessage,BatGadgetCallServices& services);
} // namespace batbox::bats
