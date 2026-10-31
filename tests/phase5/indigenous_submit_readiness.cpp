#include "interactive/submit/QueueReadiness.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace sub=batbox::interactive::submit;
int main(){
  event_loop::Loop loop;
  interactive::QueryGuard guard;
  sub::CommandQueue queue(loop);
  bool uiActive=true;
  std::vector<std::string> dispatched;
  auto readiness=std::make_shared<sub::QueueReadiness>(loop,queue,guard,
    [&]{return uiActive;},
    [&](sub::CommandList commands){
      if(commands.size()!=1)throw std::runtime_error("batch size");
      dispatched.push_back((*commands.front())["value"].get<std::string>());
      guard.reserve();
    },[](std::exception_ptr error){std::rethrow_exception(error);});
  readiness->start();
  queue.enqueue(Json{{"value","agent"},{"mode","prompt"},
    {"priority","now"},{"agentId","sub"}});
  queue.enqueue(Json{{"value","/model"},{"mode","prompt"}});
  queue.enqueue(Json{{"value","hello"},{"mode","prompt"}});
  loop.runReady();
  if(!dispatched.empty())throw std::runtime_error("local UI must block queue");
  uiActive=false;readiness->refresh();loop.runReady();
  if(dispatched!=std::vector<std::string>{"/model"}||!guard.isActive())
    throw std::runtime_error("first slash dispatch");
  guard.cancelReservation();loop.runReady();
  if(dispatched!=std::vector<std::string>{"/model","hello"}||
     !guard.isActive())throw std::runtime_error("guard release dispatch");
  guard.cancelReservation();loop.runReady();
  if(queue.size()!=1||(*queue.peek())["value"]!="agent")
    throw std::runtime_error("subagent notification isolation");
  readiness->stop();
  std::cout<<"PASS queue readiness: modal, guard, slash, subagent\n";
}
