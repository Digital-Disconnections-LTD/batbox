#include "settings/remote/Checksum.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto input=value::parse(line);
    std::cout<<value::stringify(Json{
      {"sorted",settings::remote::sortKeysDeep(input)},
      {"checksum",settings::remote::computeChecksumFromSettings(input)}})
      <<'\n';
  }
}
