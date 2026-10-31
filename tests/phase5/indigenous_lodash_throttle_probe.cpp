#include "lodash_throttle/Throttle.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>

using namespace batbox;
namespace {
struct Clock {
  struct Pending {double due;std::function<void()> callback;};
  double time=0;
  int next=1;
  std::map<int,Pending> timers;
  Json trace=Json::array();
  lodash_throttle::Services services(){
    return {
      [this]{return time;},
      [this](double delay,std::function<void()> callback){
        const int id=next++;
        trace.push_back(Json::array({"set",id,delay}));
        timers.emplace(id,Pending{time+delay,std::move(callback)});
        return lodash_throttle::Timer{[this,id]{
          trace.push_back(Json::array({"clear",id}));
          timers.erase(id);
        }};
      }
    };
  }
  void advance(double end){
    for(;;){
      auto chosen=timers.end();
      for(auto it=timers.begin();it!=timers.end();++it){
        if(it->second.due>end)continue;
        if(chosen==timers.end()||it->second.due<chosen->second.due)
          chosen=it;
      }
      if(chosen==timers.end())break;
      time=chosen->second.due;
      auto callback=std::move(chosen->second.callback);
      timers.erase(chosen);
      callback();
    }
    time=end;
  }
};
Json run(const Json& ops){
  Clock clock;
  auto wrapped=std::make_shared<lodash_throttle::Throttle>(
    [&]{clock.trace.push_back(Json::array({"invoke",clock.time}));},
    16,clock.services());
  for(const auto& op:ops){
    const auto name=op.at(0).get<std::string>();
    if(name=="call"){
      clock.time=op.at(1).get<double>();wrapped->call();
    }else if(name=="advance")clock.advance(op.at(1).get<double>());
    else if(name=="cancel")wrapped->cancel();
    else if(name=="flush")wrapped->flush();
  }
  return {{"trace",clock.trace},{"time",clock.time},
    {"pending",clock.timers.size()}};
}
}
int main(){
  const std::string input(std::istreambuf_iterator<char>(std::cin),{});
  Json output=Json::array();
  for(const auto& item:value::parse(input))output.push_back(run(item));
  std::cout<<value::stringify(output)<<'\n';
}
