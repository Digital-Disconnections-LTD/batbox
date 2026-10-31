#include "react_scheduler/Scheduler.hpp"
#include <cassert>
#include <iostream>
#include <vector>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  std::vector<std::string> trace;
  loop.queueNextTurn([&]{
    trace.push_back("first");
    loop.queueMicrotask([&]{trace.push_back("microtask");});
    loop.queueNextTurn([&]{trace.push_back("later");});
  });
  loop.queueNextTurn([&]{trace.push_back("sibling");});
  loop.queueMicrotask([&]{trace.push_back("initialMicrotask");});
  loop.runReady();
  assert((trace==std::vector<std::string>{
    "initialMicrotask","first","microtask","sibling"}));
  loop.runReady();
  assert(trace.back()=="later");
  react_scheduler::Scheduler scheduler(react_scheduler::indigenousHost(loop,
    [&](const std::string& message){trace.push_back(message);}));
  scheduler.scheduleCallback(react_scheduler::Scheduler::Normal,
    [&](bool late){
      assert(!late);trace.push_back("work");
      return react_scheduler::callback([&](bool){
        trace.push_back("continued");return react_scheduler::CallbackRef{};
      });
    });
  loop.runReady();
  assert(trace.back()=="work");
  loop.runReady();
  assert(trace.back()=="continued");
  std::cout<<"2 native next-turn and scheduler continuation checks passed\n";
}
