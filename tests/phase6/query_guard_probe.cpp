#include "interactive/QueryGuard.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
int main(){
  Json cases;std::cin>>cases;Json results=Json::array();
  for(const auto& steps:cases){
    interactive::QueryGuard guard;Json trace=Json::array();
    std::vector<std::function<void()>> stops;bool reentered=false;
    auto listener=std::make_shared<bootstrap::Signal<>::Listener>([&]{
      trace.push_back(Json::array({"notify",guard.isActive(),guard.generation()}));});
    for(const auto& step:steps){
      const auto op=step[0].get<std::string>();Json result;
      try{
        if(op=="listen")stops.push_back(guard.subscribe(listener));
        else if(op=="throwListener")stops.push_back(guard.subscribe(
          std::make_shared<bootstrap::Signal<>::Listener>([]{throw std::runtime_error("listener failed");})));
        else if(op=="reenter")stops.push_back(guard.subscribe(
          std::make_shared<bootstrap::Signal<>::Listener>([&]{
            if(!reentered){reentered=true;guard.forceEnd();guard.tryStart();}})));
        else if(op=="unlisten")stops.at(step[1].get<std::size_t>())();
        else if(op=="reserve")result=guard.reserve();
        else if(op=="cancel")guard.cancelReservation();
        else if(op=="start"){const auto gen=guard.tryStart();if(gen)result=*gen;}
        else if(op=="end")result=guard.end(step[1].get<std::uint64_t>());
        else if(op=="force")guard.forceEnd();
      }catch(const std::exception& error){result=Json{{"error",error.what()}};}
      trace.push_back(Json::array({op,result,guard.getSnapshot(),guard.generation()}));
    }
    results.push_back(trace);
  }
  std::cout<<results.dump()<<'\n';
}
