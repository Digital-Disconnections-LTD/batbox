#include "bootstrap/Signal.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <memory>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto scenario=value::parse(line).value("scenario",std::string());
    Json events=Json::array();
    if(scenario=="multi"){
      bootstrap::Signal<int,std::string> signal;
      signal.subscribe([&](const int& x,const std::string& y){
        events.push_back(std::to_string(x)+":"+y);});
      signal.emit(7,"value");
    }else{
      bootstrap::Signal<> signal;
      auto a=std::make_shared<bootstrap::Signal<>::Listener>(
        [&]{events.push_back("A");});
      auto b=std::make_shared<bootstrap::Signal<>::Listener>(
        [&]{events.push_back("B");});
      if(scenario=="oldOff"){
        auto oldOff=signal.subscribe(a);oldOff();signal.subscribe(a);
        oldOff();signal.emit();
      }else if(scenario=="duplicate"){
        auto first=signal.subscribe(a);signal.subscribe(a);
        signal.emit();first();signal.emit();
      }else if(scenario=="append"){
        signal.subscribe([&]{events.push_back("A");signal.subscribe(b);});
        signal.emit();
      }else if(scenario=="remove"){
        std::function<void()> offB;
        signal.subscribe([&]{events.push_back("A");offB();});
        offB=signal.subscribe(b);signal.emit();
      }else if(scenario=="clearOldOff"){
        auto oldOff=signal.subscribe(a);signal.clear();
        signal.subscribe(a);oldOff();signal.emit();
      }else if(scenario=="zero"){
        signal.subscribe([&]{events.push_back("zero");});signal.emit();
      }
    }
    std::cout<<value::stringify(Json{{"events",events}})<<'\n';
  }
}
