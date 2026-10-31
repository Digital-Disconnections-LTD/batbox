#include "bat_context/BatContext.hpp"
#include "bat_context/PartnerContext.hpp"
#include "event_loop/Deferred.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace batbox;
namespace {
void check(bool condition){
  if(!condition)throw std::runtime_error("Async agent-context propagation failed");
}
}
int main(){
  event_loop::Loop loop;
  event_loop::Deferred<int> promise(loop);
  auto bat=std::make_shared<bat_context::BatContext>();
  bat->batId="agent-a";bat->batType="subagent";
  auto partner=bat_context::createPartnerContext({"worker@team",
    "worker","team",std::nullopt,false,"leader",
    std::make_shared<std::stop_source>()});
  std::vector<std::string> seen;
  bat_context::runWithBatContext(bat,[&]{
    bat_context::runWithPartnerContext(partner,[&]{
      loop.queueMicrotask([&]{
        check(bat_context::getBatContext()==bat);
        check(bat_context::getPartnerContext()==partner);
        seen.push_back("microtask");
      });
      promise.then([&](const int* value,const std::exception_ptr& error){
        check(!error&&value&&*value==7);
        check(bat_context::getBatContext()==bat);
        check(bat_context::getPartnerContext()==partner);
        seen.push_back("then");
      });
    });
  });
  check(!bat_context::getBatContext());
  check(!bat_context::getPartnerContext());
  promise.resolve(7);
  loop.runReady();
  check(seen==std::vector<std::string>({"microtask","then"}));
  check(!bat_context::getBatContext());
  check(!bat_context::getPartnerContext());
  std::cout<<"native scoped agent contexts survived async reactions\n";
}
