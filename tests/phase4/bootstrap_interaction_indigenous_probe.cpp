#include "bootstrap/Interaction.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>

int main(){
  using namespace batbox;
  event_loop::Loop loop;
  auto services=bootstrap::makeIndigenousInteractionServices(loop);
  auto state=bootstrap::makeInteractionState(services);
  bootstrap::markScrollActivity(state,services);
  assert(bootstrap::getIsScrollDraining(state));
  bool resolved=false;
  auto hold=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  const auto before=std::chrono::steady_clock::now();
  bootstrap::waitForScrollIdle(loop,state,services).then(
    [&](const event_loop::Unit*,const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);
      resolved=true;hold->release();
    });
  loop.run();
  const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now()-before).count();
  assert(resolved);
  assert(!bootstrap::getIsScrollDraining(state));
  assert(elapsed>=100&&elapsed<2000);
  std::cout<<"native scroll idle timer passed\n";
}
