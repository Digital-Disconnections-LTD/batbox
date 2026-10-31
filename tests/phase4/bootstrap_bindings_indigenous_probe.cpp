#include "bootstrap/BootstrapBindings.hpp"
#include <cassert>
#include <stdexcept>

int main(){
  using namespace batbox;
  auto state=std::make_shared<bootstrap::BootstrapState>();
  double now=25;
  const auto bindings=bootstrap::bindBootstrapState(state,
    [&]{return now;},true);
  bindings.setStashedBatboxMdContent(std::string("# Project"));
  assert(state->request.stashedBatboxMdContent=="# Project");
  bindings.addInvokedSkill("main","/skills/main","body",std::nullopt);
  now=26;
  bindings.addInvokedSkill("agent","/skills/agent","agent body",
    std::string("a"));
  const auto main=bindings.getInvokedSkillsForBat(nullptr);
  const auto bat=bindings.getInvokedSkillsForBat("a");
  assert(main.size()==1&&main[0]["skillName"]=="main");
  assert(bat.size()==1&&bat[0]["skillName"]=="agent");
  assert(bat[0]["invokedAt"]==26);
  bindings.clearInvokedSkillsForBat("a");
  assert(bindings.getInvokedSkillsForBat("a").empty());
  assert(bindings.getInvokedSkillsForBat(nullptr).size()==1);
  bindings.addSlowOperation("read",200);
  assert(bootstrap::getSlowOperations(state->slow,[&]{return now;})->size()==1);
  auto matcher=std::make_shared<
    instructions::hooks::runtime::HookMatcher>();
  matcher->matcher="Read";
  instructions::hooks::runtime::MatchedHook callback;
  callback.callback=[](instructions::hooks::runtime::CallbackRequest)
    ->event_loop::Deferred<Json>{throw std::runtime_error("test callback");};
  matcher->hooks.push_back(callback);
  bootstrap::registerHookCallbacks(state->hooks,
    {{"PreToolUse",{matcher}}});
  const auto registered=bindings.getRegisteredHooksForEvent("PreToolUse");
  assert(registered&&registered->size()==1);
  assert((*registered)[0].matcher=="Read");
  assert((*registered)[0].hooks.size()==1&&
    static_cast<bool>((*registered)[0].hooks[0].callback));
  assert(!bindings.getRegisteredHooksForEvent("PostToolUse"));
  return 0;
}
