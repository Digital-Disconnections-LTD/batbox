#include "client_factory/IndigenousAuthBinding.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array();
    const auto value=client_factory::isBatboxSubscriber(
      [&]{trace.push_back("enabled");return test.value("enabled",false);},
      [&]()->std::optional<Json>{trace.push_back("tokens");
        if(!test.contains("tokens")||test["tokens"].is_null())
          return std::nullopt;
        return test["tokens"];
      });
    std::cout<<value::stringify(Json{{"value",value},
      {"trace",trace}})<<'\n';
  }
}
