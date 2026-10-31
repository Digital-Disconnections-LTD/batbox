#include "bootstrap/SlowOperations.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    bootstrap::SlowOperationsState state;
    const bool developer=test.value("developer",false);
    double now=0;
    std::shared_ptr<const bootstrap::SlowOperationList> previous;
    auto snap=[&](){
      const auto list=bootstrap::getSlowOperations(state,[&]{return now;});
      Json entries=Json::array();
      for(const auto& item:*list)
        entries.push_back({{"operation",item.operation},
          {"durationMs",item.durationMs},{"timestamp",item.timestamp}});
      Json out={{"operations",entries},
        {"sameAsPrevious",previous?Json(list==previous):Json(nullptr)}};
      previous=list;return out;
    };
    Json trace=Json::array();trace.push_back(snap());
    if(test.contains("ops"))for(const auto& op:test["ops"]){
      now=op["now"].get<double>();
      const auto kind=op["kind"].get<std::string>();
      if(kind=="add")bootstrap::addSlowOperation(state,
        op["operation"].get<std::string>(),op["duration"].get<double>(),
        developer,[&]{return now;});
      else if(kind!="get")
        throw std::runtime_error("Unknown slow-operation operation");
      trace.push_back(snap());
    }
    std::cout<<value::stringify(trace)<<'\n';
  }
}
