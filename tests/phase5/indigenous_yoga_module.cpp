#include "indigenous_yoga/Module.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

int main(){
  using namespace batbox;
  event_loop::Loop loop;
  const auto first=indigenous_yoga::yogaInstance();
  const auto promise=indigenous_yoga::loadYoga(loop);
  assert(promise.settled());
  std::vector<std::string> order;
  promise.then([&](const indigenous_yoga::YogaModuleRef* value,
      const std::exception_ptr& error){
    assert(!error&&value&&*value==first);
    order.emplace_back("then");
  });
  order.emplace_back("sync");
  loop.queueMicrotask([&]{order.emplace_back("sibling");});
  loop.runReady();
  assert((order==std::vector<std::string>{"sync","then","sibling"}));
  const bool freshNodes=first->Node.create()!=first->Node.create();
  assert(freshNodes);
  auto node=first->Node.create();
  const bool absentChild=!node->getChild(3);
  assert(absentChild);
  node->calculateLayout(10,10);
  node->free();
  bool nullStashError=false;
  std::string errorMessage;
  try{node->calculateLayout(11,10);}
  catch(const indigenous_yoga::TypeError& error){
    errorMessage=error.what();
    nullStashError=errorMessage==
      "null is not an object (evaluating 'cIn[o + 2]')";
  }
  assert(nullStashError);
  std::cout<<std::boolalpha<<order[0]<<','<<order[1]<<','<<order[2]<<'|'
    <<freshNodes<<'|'<<absentChild<<"|TypeError|"<<errorMessage<<'\n';
}
