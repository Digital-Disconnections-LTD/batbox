#include "cli/headless/InterruptedMessage.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <string>

using namespace batbox;
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const Json input=value::parse(line);
    std::vector<Json> messages=input.at("messages").get<std::vector<Json>>();
    cli::headless::removeInterruptedMessage(messages,input.at("interrupted"));
    std::cout<<value::stringify(Json(messages))<<'\n';
  }
}
