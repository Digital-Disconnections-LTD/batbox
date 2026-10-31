#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <vector>
using namespace batbox;
using namespace batbox::event_loop;
namespace {
std::vector<std::string> run(bool overlap){
  Loop loop;std::vector<std::string> trace;
  std::vector<Deferred<Unit>> drains;
  bool timerActive=false,activeDrain=false,writeQueuesNonempty=false;
  std::function<void()> schedule;
  schedule=[&]{
    if(timerActive)return;
    timerActive=true;trace.push_back("timer-set");
    loop.setTimeout(1,[&]{
      timerActive=false;activeDrain=true;trace.push_back("drain-start");
      Deferred<Unit> current(loop);drains.push_back(current);
      current.then([&,current](const Unit*,const std::exception_ptr&){
        activeDrain=false;if(writeQueuesNonempty)schedule();});
    });
  };
  schedule();schedule();loop.run();
  if(drains.size()!=1||timerActive||!activeDrain)throw std::runtime_error("first native drain state wrong");
  if(overlap){schedule();loop.run();
    if(drains.size()!=2||timerActive||!activeDrain)
      throw std::runtime_error("native scheduler did not permit concurrent drain");}
  for(const auto& drain:drains)drain.resolve({});
  loop.run();
  if(activeDrain)throw std::runtime_error("native drain did not clear activeDrain");
  return trace;
}
}
int main(){Json output=Json::array();
  for(const bool overlap:{false,true}){Json trace=Json::array();
    for(const auto& item:run(overlap))trace.push_back(item);output.push_back(trace);}
  std::cout<<value::stringify(output)<<'\n';
}
