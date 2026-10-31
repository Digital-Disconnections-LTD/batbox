#include "cli/structured/ControlMessageCompat.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <string>

using namespace batbox;
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    Json value=value::parse(line);
    cli::structured::normalizeControlMessageKeys(value);
    std::cout<<value::stringify(value)<<'\n';
  }
}
