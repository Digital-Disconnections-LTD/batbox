#include "cron/use_scheduled/FireTime.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <string>

int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const auto input=batbox::value::parse(line);
    const auto value=batbox::cron::use_scheduled::formatCronFireTime(
      input.at("epochMs").get<double>());
    std::cout<<batbox::value::stringify(batbox::Json{{"value",value}})<<'\n';
  }
}
