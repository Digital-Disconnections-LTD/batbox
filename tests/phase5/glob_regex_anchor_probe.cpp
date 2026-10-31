#include "glob/Regex16.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
int main(){using namespace batbox;
  const std::string bytes((std::istreambuf_iterator<char>(std::cin)),{});
  const auto cases=value::parse(bytes);Json results=Json::array();
  for(const auto& item:cases){
    const glob::Regex16 regex(value::utf16CodeUnits(item.at("pattern").get<std::string>()));
    results.push_back(regex.search(value::utf16CodeUnits(item.at("text").get<std::string>())));
  }
  std::cout<<results.dump()<<'\n';
}
