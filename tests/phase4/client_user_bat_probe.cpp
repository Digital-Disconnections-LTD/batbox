#include "client_factory/UserBat.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    std::cout<<value::stringify(client_factory::getBatboxUserBat(
      test.at("version").get<std::string>()))<<'\n';
  }
}
