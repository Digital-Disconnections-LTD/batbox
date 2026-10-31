#include "cli/structured/OutputWrite.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <string>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  std::string line;
  while(std::getline(std::cin,line)){
    const Json input=value::parse(line);
    Json writes=Json::array();
    auto pending=cli::structured::write(loop,input,
      [](const Json& value){return std::optional<std::string>(
        value::stringify(value));},
      [&](const std::string& text){writes.push_back(text);});
    std::exception_ptr error;
    pending.then([&](const event_loop::Unit*,const auto& failure){
      error=failure;
    });
    loop.runReady();
    if(error)std::rethrow_exception(error);
    std::cout<<value::stringify(writes)<<'\n';
  }
}
