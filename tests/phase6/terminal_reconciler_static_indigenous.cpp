#include "terminal_reconciler/HostStatic.hpp"
#include "event_loop/Loop.hpp"
#include <cassert>
#include <iostream>

int main(){
  using namespace batbox;
  event_loop::Loop loop;
  terminal_events::Dispatcher dispatcher({1,2,9,0},
    [](std::exception_ptr){std::terminate();});
  terminal_reconciler::HostStatic host(
    terminal_reconciler::indigenousHostStaticImports(dispatcher,loop));
  int fired=0;
  const auto due=host.scheduleTimeout([&]{++fired;},1);
  const auto cancelled=host.scheduleTimeout([&]{fired+=10;},10000);
  host.cancelTimeout(cancelled);
  loop.run();
  assert(due&&cancelled&&fired==1);
  std::cout<<"1 native owner-loop reconciler timer/cancel trace passed\n";
}
