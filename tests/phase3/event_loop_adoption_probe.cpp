#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <string>
#include <vector>
using namespace batbox;
using namespace batbox::event_loop;
namespace {
void tick(Loop& loop,std::vector<std::string>& trace,int index){
  trace.push_back("tick"+std::to_string(index));
  if(index<6)loop.queueMicrotask([&loop,&trace,index]{tick(loop,trace,index+1);});
}
Json run(bool direct,bool settled){
  Loop loop;std::vector<std::string> trace;Deferred<int> input(loop);
  if(settled)input.resolve(7);
  auto output=direct?adoptAsyncReturn(input):[&]{
    Deferred<int> awaited(loop);
    input.then([awaited](const int* value,const std::exception_ptr& error){
      if(error)awaited.reject(error);else awaited.resolve(*value);
    });
    return awaited;
  }();
  output.then([&trace](const int*,const std::exception_ptr&){trace.push_back("done");});
  loop.queueMicrotask([&]{trace.push_back("sibling");});
  loop.queueMicrotask([&]{tick(loop,trace,0);});
  if(!settled)loop.queueMicrotask([input]{input.resolve(7);});
  loop.runReady();Json value=Json::array();
  for(const auto& item:trace)value.push_back(item);
  return value;
}
}
int main(){Json output=Json::object();
  output["directSettled"]=run(true,true);
  output["directPending"]=run(true,false);
  output["awaitedSettled"]=run(false,true);
  output["awaitedPending"]=run(false,false);
  std::cout<<value::stringify(output)<<'\n';}
